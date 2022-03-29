# DualSense Control

Linux tool for controlling Sony PlayStation 5 DualSense controller.

    Usage: dualsensectl [options] command [ARGS]

    Options:
      -l                                       List available devices
      -d DEVICE                                Specify which device to use
      -h --help                                Show this help message
      -v --version                             Show version
    Commands:
      power-off                                Turn off the controller (BT only)
      battery                                  Get the controller battery level
      lightbar STATE                           Enable (on) or disable (off) lightbar
      lightbar RED GREEN BLUE [BRIGHTNESS]     Set lightbar color and brightness (0-255)
      player-leds NUMBER                       Set player LEDs (1-5) or disabled (0)
      microphone STATE                         Enable (on) or disable (off) microphone
      microphone-led STATE                     Enable (on) or disable (off) microphone LED


AUR: [dualsensectl-git](https://aur.archlinux.org/packages/dualsensectl-git/)

### Dependencies

* libhidapi-hidraw
* libdbus-1

### Building

    make && make install

### udev rules

Also installed by Steam, so you may already have it configured. If not, create `/etc/udev/rules.d/70-dualsensectl.rules`:

    # PS5 DualSense controller over USB hidraw
    KERNEL=="hidraw*", ATTRS{idVendor}=="054c", ATTRS{idProduct}=="0ce6", MODE="0660", TAG+="uaccess"

    # PS5 DualSense controller over bluetooth hidraw
    KERNEL=="hidraw*", KERNELS=="*054C:0CE6*", MODE="0660", TAG+="uaccess"
