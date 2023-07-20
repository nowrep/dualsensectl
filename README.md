# DualSense Control [DualSenseCTL]
- _Latest version 0.3_

Linux tool for controlling Sony PlayStation 5 DualSense controller.

    Usage: dualsensectl [options] command [ARGUMENTS]
    
    Options:
      -l                                          List available devices [xx:xx:xx:xx:xx:xx] (USB/Bluetooth)
      -d [DEVICE]                                 Specify which device to use [xx:xx:xx:xx:xx:xx] (USB/Bluetooth)
      -w                                          Wait for shell command to complete (Monitor only)
      -h --help                                   Shows this help message
      -v --version                                Shows version
    Commands:
      power-off                                   Turn off the controller (Bluetooth only)
      battery                                     Get the controller battery level and charging/discharging information
      lightbar [STATE]                            Enable [ON] or disable [OFF] lightbar
      lightbar [RED] [GREEN] [BLUE] [BRIGHTNESS]  Set lightbar color and brightness [0-255] [0-255] [0-255] [0-255]
      player-leds [NUMBER]                        Set player LEDs [1-5] or disabled [0]
      microphone [STATE]                          Enable [ON] or disable [OFF] microphone
      microphone-led [STATE]                      Enable [ON] or disable [OFF] microphone orange LED
      monitor [add COMMAND] / [remove COMMAND]    Run shell command [COMMAND] on add/remove events

## Building from SOURCE (gcc)

`./`  
`~ sudo make && sudo make install`

## Download sources:
- Arch Linux - AUR: [dualsensectl] (https://aur.archlinux.org/packages/dualsensectl)
- Arch Linux - AUR: [dualsensectl-git] (https://aur.archlinux.org/packages/dualsensectl-git/) -- GIT version
- Debian/Ubuntu - DEB: [dualsensectl] ---  
- openSUSE - RPM: [dualsensectl] (https://build.opensuse.org/package/show/home:MartinVonReichenberg:hardware/dualsensectl)
- Fedora - RPM: [dualsensectl] (https://copr.fedorainfracloud.org/coprs/birkch/dualsensectl/)
- Mageia - RPM: ---
## Make Dependencies

### GENERIC (Gcc/PkgConf)
* gcc/musl | systemd-dev/systemd-devel

### Arch Linux
* gcc | dbus| systemd | systemd-libs

### Debian/Ubuntu
* gcc | dbus | libdbus-1-dev | libhidapi-dev | libudev-dev

### openSUSE
* gcc-devel | gcc-c++ | dbus-1-devel | libdbus-c++-devel | libhidapi-devel | libudev-devel
 
### Fedora
* gcc | gcc-c++ | dbus-devel | hidapi-devel | systemd-devel

### Mageia
* gcc | gcc-c++ | lib64dbus-devel | lib64dbus-c++-devel | lib64hidapi-devel | lib64udev-devel

### KaOS
* gcc | dbus | systemd 

## Dependencies

### GENERIC (Gcc/PkgConf)
* gcc/musl | dbus | hidapi/hidapi-hidraw | udev/libudev

### Arch Linux
* gcc | dbus | dbus-c++ | systemd-libs | hidapi | libudev0-shim

### Debian/Ubuntu
* gcc | dbus | libdbus-1-3 | libhidapi-hidraw0 | libudev0 | libudev1

### openSUSE
* gcc | gcc-c++ | dbus-1 | libhidapi-hidraw0 | libhidapi-libusb0 | udev | libudev1
 
### Fedora
* gcc | gcc-c++ | dbus | systemd | hidapi | systemd-devel

### Mageia
* gcc | gcc-c++ | lib64dbus1_3 | lib64hidapi0 | lib64udev1

### KaOS
* gcc-libs | dbus | systemd 

## udev rules (Optional)
#### _No longer neccessary to set manually; Included in installation_ . . .

Also installed by Steam, so you may already have it configured. If not, create file `/etc/udev/rules.d/70-dualsensectl.rules`:

    # PS5 DualSense controller over USB hidraw
    KERNEL=="hidraw*", ATTRS{idVendor}=="054c", ATTRS{idProduct}=="0ce6", MODE="0660", TAG+="uaccess"

    # PS5 DualSense controller over bluetooth hidraw
    KERNEL=="hidraw*", KERNELS=="*054C:0CE6*", MODE="0660", TAG+="uaccess"
