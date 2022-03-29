// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  HID driver for Sony DualSense(TM) controller.
 *
 *  Copyright (c) 2020 Sony Interactive Entertainment
 */

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <dbus/dbus.h>
#include <hidapi/hidapi.h>

#include "crc32.h"

#define DS_VENDOR_ID 0x054c
#define DS_PRODUCT_ID 0x0ce6

/* Seed values for DualShock4 / DualSense CRC32 for different report types. */
#define PS_INPUT_CRC32_SEED 0xA1
#define PS_OUTPUT_CRC32_SEED 0xA2
#define PS_FEATURE_CRC32_SEED 0xA3

#define DS_INPUT_REPORT_USB 0x01
#define DS_INPUT_REPORT_USB_SIZE 64
#define DS_INPUT_REPORT_BT 0x31
#define DS_INPUT_REPORT_BT_SIZE 78
#define DS_OUTPUT_REPORT_USB 0x02
#define DS_OUTPUT_REPORT_USB_SIZE 63
#define DS_OUTPUT_REPORT_BT 0x31
#define DS_OUTPUT_REPORT_BT_SIZE 78

#define DS_FEATURE_REPORT_CALIBRATION 0x05
#define DS_FEATURE_REPORT_CALIBRATION_SIZE 41
#define DS_FEATURE_REPORT_PAIRING_INFO 0x09
#define DS_FEATURE_REPORT_PAIRING_INFO_SIZE 20
#define DS_FEATURE_REPORT_FIRMWARE_INFO 0x20
#define DS_FEATURE_REPORT_FIRMWARE_INFO_SIZE 64

/* Magic value required in tag field of Bluetooth output report. */
#define DS_OUTPUT_TAG 0x10
/* Flags for DualSense output report. */
#define BIT(n) (1 << n)
#define DS_OUTPUT_VALID_FLAG0_COMPATIBLE_VIBRATION BIT(0)
#define DS_OUTPUT_VALID_FLAG0_HAPTICS_SELECT BIT(1)
#define DS_OUTPUT_VALID_FLAG1_MIC_MUTE_LED_CONTROL_ENABLE BIT(0)
#define DS_OUTPUT_VALID_FLAG1_POWER_SAVE_CONTROL_ENABLE BIT(1)
#define DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE BIT(2)
#define DS_OUTPUT_VALID_FLAG1_RELEASE_LEDS BIT(3)
#define DS_OUTPUT_VALID_FLAG1_PLAYER_INDICATOR_CONTROL_ENABLE BIT(4)
#define DS_OUTPUT_VALID_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE BIT(1)
#define DS_OUTPUT_POWER_SAVE_CONTROL_MIC_MUTE BIT(4)
#define DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_ON BIT(0)
#define DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT BIT(1)

/* Status field of DualSense input report. */
#define DS_STATUS_BATTERY_CAPACITY 0xF
#define DS_STATUS_CHARGING 0xF0
#define DS_STATUS_CHARGING_SHIFT 4

struct dualsense_touch_point {
    uint8_t contact;
    uint8_t x_lo;
    uint8_t x_hi:4, y_lo:4;
    uint8_t y_hi;
} __attribute__((packed));

/* Main DualSense input report excluding any BT/USB specific headers. */
struct dualsense_input_report {
    uint8_t x, y;
    uint8_t rx, ry;
    uint8_t z, rz;
    uint8_t seq_number;
    uint8_t buttons[4];
    uint8_t reserved[4];

    /* Motion sensors */
    uint16_t gyro[3]; /* x, y, z */
    uint16_t accel[3]; /* x, y, z */
    uint32_t sensor_timestamp;
    uint8_t reserved2;

    /* Touchpad */
    struct dualsense_touch_point points[2];

    uint8_t reserved3[12];
    uint8_t status;
    uint8_t reserved4[10];
} __attribute__((packed));

/* Common data between DualSense BT/USB main output report. */
struct dualsense_output_report_common {
    uint8_t valid_flag0;
    uint8_t valid_flag1;

    /* For DualShock 4 compatibility mode. */
    uint8_t motor_right;
    uint8_t motor_left;

    /* Audio controls */
    uint8_t reserved[4];
    uint8_t mute_button_led;

    uint8_t power_save_control;
    uint8_t reserved2[28];

    /* LEDs and lightbar */
    uint8_t valid_flag2;
    uint8_t reserved3[2];
    uint8_t lightbar_setup;
    uint8_t led_brightness;
    uint8_t player_leds;
    uint8_t lightbar_red;
    uint8_t lightbar_green;
    uint8_t lightbar_blue;
} __attribute__((packed));

struct dualsense_output_report_bt {
    uint8_t report_id; /* 0x31 */
    uint8_t seq_tag;
    uint8_t tag;
    struct dualsense_output_report_common common;
    uint8_t reserved[24];
    uint32_t crc32;
} __attribute__((packed));

struct dualsense_output_report_usb {
    uint8_t report_id; /* 0x02 */
    struct dualsense_output_report_common common;
    uint8_t reserved[15];
} __attribute__((packed));

/*
 * The DualSense has a main output report used to control most features. It is
 * largely the same between Bluetooth and USB except for different headers and CRC.
 * This structure hide the differences between the two to simplify sending output reports.
 */
struct dualsense_output_report {
    uint8_t *data; /* Start of data */
    uint8_t len; /* Size of output report */

    /* Points to Bluetooth data payload in case for a Bluetooth report else NULL. */
    struct dualsense_output_report_bt *bt;
    /* Points to USB data payload in case for a USB report else NULL. */
    struct dualsense_output_report_usb *usb;
    /* Points to common section of report, so past any headers. */
    struct dualsense_output_report_common *common;
};

struct dualsense {
    bool bt;
    hid_device *dev;
    char mac_address[18];
    uint8_t output_seq;
};

static void dualsense_init_output_report(struct dualsense *ds, struct dualsense_output_report *rp, void *buf)
{
    if (ds->bt) {
        struct dualsense_output_report_bt *bt = buf;

        memset(bt, 0, sizeof(*bt));
        bt->report_id = DS_OUTPUT_REPORT_BT;
        bt->tag = DS_OUTPUT_TAG; /* Tag must be set. Exact meaning is unclear. */

        /*
         * Highest 4-bit is a sequence number, which needs to be increased
         * every report. Lowest 4-bit is tag and can be zero for now.
         */
        bt->seq_tag = (ds->output_seq << 4) | 0x0;
        if (++ds->output_seq == 16)
            ds->output_seq = 0;

        rp->data = buf;
        rp->len = sizeof(*bt);
        rp->bt = bt;
        rp->usb = NULL;
        rp->common = &bt->common;
    } else { /* USB */
        struct dualsense_output_report_usb *usb = buf;

        memset(usb, 0, sizeof(*usb));
        usb->report_id = DS_OUTPUT_REPORT_USB;

        rp->data = buf;
        rp->len = sizeof(*usb);
        rp->bt = NULL;
        rp->usb = usb;
        rp->common = &usb->common;
    }
}

static void dualsense_send_output_report(struct dualsense *ds, struct dualsense_output_report *report)
{
    /* Bluetooth packets need to be signed with a CRC in the last 4 bytes. */
    if (report->bt) {
        uint32_t crc;
        uint8_t seed = PS_OUTPUT_CRC32_SEED;

        crc = crc32_le(0xFFFFFFFF, &seed, 1);
        crc = ~crc32_le(crc, report->data, report->len - 4);

        report->bt->crc32 = crc;
    }

    int res = hid_write(ds->dev, report->data, report->len);
    if (res < 0) {
        fprintf(stderr, "Error: %ls\n", hid_error(ds->dev));
    }
}

static bool compare_serial(const char *s, const wchar_t *dev)
{
    if (!s) {
        return true;
    }
    const size_t len = wcslen(dev);
    if (strlen(s) != len) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (s[i] != dev[i]) {
            return false;
        }
    }
    return true;
}

static bool dualsense_init(struct dualsense *ds, const char *serial)
{
    bool ret = false;

    memset(ds, 0, sizeof(*ds));

    bool found = false;
    struct hid_device_info *devs = hid_enumerate(DS_VENDOR_ID, DS_PRODUCT_ID);
    struct hid_device_info *dev = devs;
    while (dev) {
        if (compare_serial(serial, dev->serial_number)) {
            found = true;
            break;
        }
        dev = dev->next;
    }

    if (!found) {
        if (serial) {
            fprintf(stderr, "Device '%s' not found\n", serial);
        } else {
            fprintf(stderr, "No device found\n");
        }
        ret = false;
        goto out;
    }

    if (wcslen(dev->serial_number) != 17) {
        fprintf(stderr, "Invalid device serial number: %ls\n", dev->serial_number);
        ret = false;
        goto out;
    }

    ds->dev = hid_open(DS_VENDOR_ID, DS_PRODUCT_ID, dev->serial_number);
    if (!ds->dev) {
        fprintf(stderr, "Failed to open device: %ls\n", hid_error(NULL));
        ret = false;
        goto out;
    }

    for (int i = 0; i < 18; ++i) {
        char c = dev->serial_number[i];
        if (c && (i + 1) % 3) {
            c = toupper(c);
        }
        ds->mac_address[i] = c;
    }

    ds->bt = dev->interface_number == -1;

    ret = true;

out:
    if (devs) {
        hid_free_enumeration(devs);
    }
    return ret;
}

static void dualsense_destroy(struct dualsense *ds)
{
    hid_close(ds->dev);
}

static bool dualsense_bt_disconnect(struct dualsense *ds)
{
    DBusError err;
    dbus_error_init(&err);
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Failed to connect to DBus daemon: %s %s\n", err.name, err.message);
        return false;
    }
    DBusMessage *msg = dbus_message_new_method_call("org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    dbus_message_unref(msg);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Failed to enumerate BT devices: %s %s\n", err.name, err.message);
        return false;
    }
    DBusMessageIter dict;
    dbus_message_iter_init(reply, &dict);
    int objects_count = dbus_message_iter_get_element_count(&dict);
    DBusMessageIter dict_entry;
    dbus_message_iter_recurse(&dict, &dict_entry);
    DBusMessageIter dict_kv;
    char *ds_path = NULL;
    char *path, *iface, *prop;
    while (objects_count-- && !ds_path) {
        dbus_message_iter_recurse(&dict_entry, &dict_kv);
        dbus_message_iter_get_basic(&dict_kv, &path);
        dbus_message_iter_next(&dict_kv);
        int ifaces_count = dbus_message_iter_get_element_count(&dict_kv);
        DBusMessageIter ifacedict_entry, ifacedict_kv;
        dbus_message_iter_recurse(&dict_kv, &ifacedict_entry);
        while (ifaces_count-- && !ds_path) {
            dbus_message_iter_recurse(&ifacedict_entry, &ifacedict_kv);
            dbus_message_iter_get_basic(&ifacedict_kv, &iface);
            if (!strcmp(iface, "org.bluez.Device1")) {
                dbus_message_iter_next(&ifacedict_kv);
                int props_count = dbus_message_iter_get_element_count(&ifacedict_kv);
                DBusMessageIter propdict_entry, propdict_kv;
                dbus_message_iter_recurse(&ifacedict_kv, &propdict_entry);
                char *address = NULL;
                int connected = 0;
                while (props_count-- && !ds_path) {
                    dbus_message_iter_recurse(&propdict_entry, &propdict_kv);
                    dbus_message_iter_get_basic(&propdict_kv, &prop);
                    DBusMessageIter variant;
                    if (!strcmp(prop, "Address")) {
                        dbus_message_iter_next(&propdict_kv);
                        dbus_message_iter_recurse(&propdict_kv, &variant);
                        dbus_message_iter_get_basic(&variant, &address);
                    } else if (!strcmp(prop, "Connected")) {
                        dbus_message_iter_next(&propdict_kv);
                        dbus_message_iter_recurse(&propdict_kv, &variant);
                        dbus_message_iter_get_basic(&variant, &connected);
                    }
                    dbus_message_iter_next(&propdict_entry);
                }
                if (connected && address && !strcmp(address, ds->mac_address) && !ds_path) {
                    ds_path = path;
                }
            }
            dbus_message_iter_next(&ifacedict_entry);
        }
        dbus_message_iter_next(&dict_entry);
    }
    dbus_message_unref(reply);
    if (!ds_path) {
        fprintf(stderr, "Failed to find BT device\n");
        return false;
    }
    msg = dbus_message_new_method_call("org.bluez", ds_path, "org.bluez.Device1", "Disconnect");
    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    dbus_message_unref(msg);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Failed to disconnect BT device: %s %s\n", err.name, err.message);
        return false;
    }
    dbus_message_unref(reply);
    dbus_connection_unref(conn);
    return true;
}

static int command_power_off(struct dualsense *ds)
{
    if (!ds->bt) {
        fprintf(stderr, "Controller is not connected via BT\n");
        return 1;
    }
    if (!dualsense_bt_disconnect(ds)) {
        return 2;
    }
    return 0;
}

static int command_battery(struct dualsense *ds)
{
    uint8_t data[DS_INPUT_REPORT_BT_SIZE];
    int res = hid_read_timeout(ds->dev, data, sizeof(data), 1000);
    if (res <= 0) {
        if (res == 0) {
            fprintf(stderr, "Timeout waiting for report\n");
        } else {
            fprintf(stderr, "Failed to read report %ls\n", hid_error(ds->dev));
        }
        return 2;
    }

    struct dualsense_input_report *ds_report;

    if (!ds->bt && data[0] == DS_INPUT_REPORT_USB && res == DS_INPUT_REPORT_USB_SIZE) {
        ds_report = (struct dualsense_input_report *)&data[1];
    } else if (ds->bt && data[0] == DS_INPUT_REPORT_BT && res == DS_INPUT_REPORT_BT_SIZE) {
        /* Last 4 bytes of input report contain crc32 */
        /* uint32_t report_crc = *(uint32_t*)&data[res - 4]; */
        ds_report = (struct dualsense_input_report *)&data[2];
    } else {
        fprintf(stderr, "Unhandled report ID %d\n", (int)data[0]);
        return 3;
    }

    const char *battery_status;
    uint8_t battery_capacity;
    uint8_t battery_data = ds_report->status & DS_STATUS_BATTERY_CAPACITY;
    uint8_t charging_status = (ds_report->status & DS_STATUS_CHARGING) >> DS_STATUS_CHARGING_SHIFT;

#define min(a, b) ((a) < (b) ? (a) : (b))
    switch (charging_status) {
    case 0x0:
        /*
         * Each unit of battery data corresponds to 10%
         * 0 = 0-9%, 1 = 10-19%, .. and 10 = 100%
         */
        battery_capacity = min(battery_data * 10 + 5, 100);
        battery_status = "discharging";
        break;
    case 0x1:
        battery_capacity = 100;
        battery_status = "full";
        break;
    case 0x2:
        battery_capacity = min(battery_data * 10 + 5, 100);
        battery_status = "charging";
        break;
    case 0xa: /* voltage or temperature out of range */
    case 0xb: /* temperature error */
        battery_capacity = 0;
        battery_status = "not-charging";
        break;
    case 0xf: /* charging error */
    default:
        battery_capacity = 0;
        battery_status = "unknown";
    }
#undef min

    printf("%d %s\n", (int)battery_capacity, battery_status);
    return 0;
}

static int command_lightbar1(struct dualsense *ds, char *state)
{
    struct dualsense_output_report rp;
    uint8_t rbuf[DS_OUTPUT_REPORT_BT_SIZE];
    dualsense_init_output_report(ds, &rp, rbuf);

    rp.common->valid_flag2 = DS_OUTPUT_VALID_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE;
    if (!strcmp(state, "on")) {
        rp.common->lightbar_setup = DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_ON;
    } else if (!strcmp(state, "off")) {
        rp.common->lightbar_setup = DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT;
    } else {
        fprintf(stderr, "Invalid state\n");
        return 1;
    }

    dualsense_send_output_report(ds, &rp);

    return 0;
}

static int command_lightbar3(struct dualsense *ds, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
    struct dualsense_output_report rp;
    uint8_t rbuf[DS_OUTPUT_REPORT_BT_SIZE];
    dualsense_init_output_report(ds, &rp, rbuf);

    uint8_t max_brightness = 255;

    rp.common->valid_flag1 = DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE;
    rp.common->lightbar_red = brightness * red / max_brightness;
    rp.common->lightbar_green = brightness * green / max_brightness;
    rp.common->lightbar_blue = brightness * blue / max_brightness;

    dualsense_send_output_report(ds, &rp);

    return 0;
}

static int command_player_leds(struct dualsense *ds, uint8_t number)
{
    if (number > 5) {
        fprintf(stderr, "Invalid player number\n");
        return 1;
    }

    struct dualsense_output_report rp;
    uint8_t rbuf[DS_OUTPUT_REPORT_BT_SIZE];
    dualsense_init_output_report(ds, &rp, rbuf);

    static const int player_ids[6] = {
        0,
        BIT(2),
        BIT(3) | BIT(1),
        BIT(4) | BIT(2) | BIT(0),
        BIT(4) | BIT(3) | BIT(1) | BIT(0),
        BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)
    };

    rp.common->valid_flag1 = DS_OUTPUT_VALID_FLAG1_PLAYER_INDICATOR_CONTROL_ENABLE;
    rp.common->player_leds = player_ids[number];

    dualsense_send_output_report(ds, &rp);

    return 0;
}

static int command_microphone(struct dualsense *ds, char *state)
{
    struct dualsense_output_report rp;
    uint8_t rbuf[DS_OUTPUT_REPORT_BT_SIZE];
    dualsense_init_output_report(ds, &rp, rbuf);

    rp.common->valid_flag1 = DS_OUTPUT_VALID_FLAG1_POWER_SAVE_CONTROL_ENABLE;
    if (!strcmp(state, "on")) {
        rp.common->power_save_control &= ~DS_OUTPUT_POWER_SAVE_CONTROL_MIC_MUTE;
    } else if (!strcmp(state, "off")) {
        rp.common->power_save_control |= DS_OUTPUT_POWER_SAVE_CONTROL_MIC_MUTE;
    } else {
        fprintf(stderr, "Invalid state\n");
        return 1;
    }

    dualsense_send_output_report(ds, &rp);

    return 0;
}

static int command_microphone_led(struct dualsense *ds, char *state)
{
    struct dualsense_output_report rp;
    uint8_t rbuf[DS_OUTPUT_REPORT_BT_SIZE];
    dualsense_init_output_report(ds, &rp, rbuf);

    rp.common->valid_flag1 = DS_OUTPUT_VALID_FLAG1_MIC_MUTE_LED_CONTROL_ENABLE;
    if (!strcmp(state, "on")) {
        rp.common->mute_button_led = 1;
    } else if (!strcmp(state, "off")) {
        rp.common->mute_button_led = 0;
    } else {
        fprintf(stderr, "Invalid state\n");
        return 1;
    }

    dualsense_send_output_report(ds, &rp);

    return 0;
}

static void print_help()
{
    printf("Usage: dualsensectl [options] command [ARGS]\n");
    printf("\n");
    printf("Options:\n");
    printf("  -l                                       List available devices\n");
    printf("  -d DEVICE                                Specify which device to use\n");
    printf("  -h --help                                Show this help message\n");
    printf("  -v --version                             Show version\n");
    printf("Commands:\n");
    printf("  power-off                                Turn off the controller (BT only)\n");
    printf("  battery                                  Get the controller battery level\n");
    printf("  lightbar STATE                           Enable (on) or disable (off) lightbar\n");
    printf("  lightbar RED GREEN BLUE [BRIGHTNESS]     Set lightbar color and brightness (0-255)\n");
    printf("  player-leds NUMBER                       Set player LEDs (1-5) or disabled (0)\n");
    printf("  microphone STATE                         Enable (on) or disable (off) microphone\n");
    printf("  microphone-led STATE                     Enable (on) or disable (off) microphone LED\n");
}

static void print_version()
{
    printf("%s\n", DUALSENSECTL_VERSION);
}

static int list_devices()
{
    struct hid_device_info *devs = hid_enumerate(DS_VENDOR_ID, DS_PRODUCT_ID);
    if (!devs) {
        fprintf(stderr, "No devices found\n");
        return 1;
    }
    printf("Devices:\n");
    struct hid_device_info *dev = devs;
    while (dev) {
        printf(" %ls (%s)\n", dev->serial_number, dev->interface_number == -1 ? "Bluetooth" : "USB");
        dev = dev->next;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_help();
        return 1;
    }

    const char *dev_serial = NULL;

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        print_help();
        return 0;
    } else if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
        print_version();
        return 0;
    } else if (!strcmp(argv[1], "-l")) {
        return list_devices();
    } else if (!strcmp(argv[1], "-d")) {
        if (argc < 3) {
            print_help();
            return 1;
        }
        dev_serial = argv[2];
        argc -= 2;
        argv += 2;
    }

    struct dualsense ds;
    if (!dualsense_init(&ds, dev_serial)) {
        return 1;
    }

    if (!strcmp(argv[1], "power-off")) {
        return command_power_off(&ds);
    } else if (!strcmp(argv[1], "battery")) {
        return command_battery(&ds);
    } else if (!strcmp(argv[1], "lightbar")) {
        if (argc == 3) {
            return command_lightbar1(&ds, argv[2]);
        } else if (argc == 5 || argc == 6) {
            uint8_t brightness = argc == 6 ? atoi(argv[5]) : 255;
            return command_lightbar3(&ds, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), brightness);
        } else {
            fprintf(stderr, "Invalid arguments\n");
            return 2;
        }
    } else if (!strcmp(argv[1], "player-leds")) {
        if (argc != 3) {
            fprintf(stderr, "Invalid arguments\n");
            return 2;
        }
        return command_player_leds(&ds, atoi(argv[2]));
    } else if (!strcmp(argv[1], "microphone")) {
        if (argc != 3) {
            fprintf(stderr, "Invalid arguments\n");
            return 2;
        }
        return command_microphone(&ds, argv[2]);
    } else if (!strcmp(argv[1], "microphone-led")) {
        if (argc != 3) {
            fprintf(stderr, "Invalid arguments\n");
            return 2;
        }
        return command_microphone_led(&ds, argv[2]);
    } else {
        fprintf(stderr, "Invalid command\n");
        return 2;
    }

    dualsense_destroy(&ds);
    return 0;
}
