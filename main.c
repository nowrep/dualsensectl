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
    uint8_t mac_address[6]; /* little endian order */
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

static bool dualsense_init(struct dualsense *ds)
{
    memset(ds, 0, sizeof(*ds));

    ds->dev = hid_open(DS_VENDOR_ID, DS_PRODUCT_ID, NULL);
    if (!ds->dev) {
        fprintf(stderr, "Failed to open device: %ls\n", hid_error(NULL));
        return false;
    }

    uint8_t buf[DS_FEATURE_REPORT_PAIRING_INFO_SIZE];
    buf[0] = DS_FEATURE_REPORT_PAIRING_INFO;
    int res = hid_get_feature_report(ds->dev, buf, sizeof(buf));
    if (res != sizeof(buf)) {
        fprintf(stderr, "Invalid feature report\n");
        return false;
    }

    memcpy(ds->mac_address, &buf[1], sizeof(ds->mac_address));
    ds->bt = (uint32_t)buf[16] != 0;

    return true;
}

static void dualsense_destroy(struct dualsense *ds)
{
    hid_close(ds->dev);
}

static bool dualsense_bt_disconnect(struct dualsense *ds)
{
    char ds_mac[18];
    snprintf(ds_mac, 18, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", ds->mac_address[5], ds->mac_address[4], ds->mac_address[3],
             ds->mac_address[2], ds->mac_address[1], ds->mac_address[0]);

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
    while (objects_count--) {
        dbus_message_iter_recurse(&dict_entry, &dict_kv);
        dbus_message_iter_get_basic(&dict_kv, &path);
        dbus_message_iter_next(&dict_kv);
        int ifaces_count = dbus_message_iter_get_element_count(&dict_kv);
        DBusMessageIter ifacedict_entry, ifacedict_kv;
        dbus_message_iter_recurse(&dict_kv, &ifacedict_entry);
        while (ifaces_count--) {
            dbus_message_iter_recurse(&ifacedict_entry, &ifacedict_kv);
            dbus_message_iter_get_basic(&ifacedict_kv, &iface);
            if (!strcmp(iface, "org.bluez.Device1")) {
                dbus_message_iter_next(&ifacedict_kv);
                int props_count = dbus_message_iter_get_element_count(&ifacedict_kv);
                DBusMessageIter propdict_entry, propdict_kv;
                dbus_message_iter_recurse(&ifacedict_kv, &propdict_entry);
                char *address = NULL;
                bool connected = 0;
                while (props_count--) {
                    dbus_message_iter_recurse(&propdict_entry, &propdict_kv);
                    dbus_message_iter_get_basic(&propdict_kv, &prop);
                    DBusMessageIter variant;
                    if (!strcmp(prop, "Connected")) {
                        dbus_message_iter_next(&propdict_kv);
                        dbus_message_iter_recurse(&propdict_kv, &variant);
                        dbus_message_iter_get_basic(&variant, &connected);
                    } else if (!strcmp(prop, "Address")) {
                        dbus_message_iter_next(&propdict_kv);
                        dbus_message_iter_recurse(&propdict_kv, &variant);
                        dbus_message_iter_get_basic(&variant, &address);
                    }
                    dbus_message_iter_next(&propdict_entry);
                }
                if (!strcmp(address, ds_mac) && connected && !ds_path) {
                    ds_path = path;
                    break;
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

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: dualsensectl command [ARGS]\n");
        printf("\n");
        printf("Commands:\n");
        printf("  power-off                                Turn off the controller (BT only)\n");
        printf("  lightbar STATE                           Enable (on) or disable (off) lightbar\n");
        printf("  lightbar RED GREEN BLUE [BRIGHTNESS]     Set lightbar color and brightness (0-255)\n");
        printf("  player-leds NUMBER                       Set player LEDs (1-5) or disabled (0)\n");
        printf("  microphone STATE                         Enable (on) or disable (off) microphone\n");
        return 1;
    }

    struct dualsense ds;
    if (!dualsense_init(&ds)) {
        return 1;
    }

    if (!strcmp(argv[1], "power-off")) {
        return command_power_off(&ds);
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
    } else {
        fprintf(stderr, "Invalid command\n");
        return 2;
    }

    dualsense_destroy(&ds);
    return 0;
}
