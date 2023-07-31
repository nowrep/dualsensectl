# DualSense Control

Linux tool for controlling Sony PlayStation 5 DualSense controller.

    Usage: dualsensectl [options] command [ARGS]

    Options:
      -l                                       List available devices
      -d DEVICE                                Specify which device to use
      -w                                       Wait for shell command to complete (monitor only)
      -h --help                                Show this help message
      -v --version                             Show version
    Commands:
      power-off                                Turn off the controller (BT only)
      battery                                  Get the controller battery level
      info                                     Get the controller firmware info
      lightbar STATE                           Enable (on) or disable (off) lightbar
      lightbar RED GREEN BLUE [BRIGHTNESS]     Set lightbar color and brightness (0-255)
      player-leds NUMBER                       Set player LEDs (1-5) or disabled (0)
      microphone STATE                         Enable (on) or disable (off) microphone
      microphone-led STATE                     Enable (on) or disable (off) microphone LED
      speaker STATE                            Toggle to 'internal' speaker, 'headphone' or both
      volume VOLUME                            Set audio volume (0-255) of internal speaker and headphone
      attenuation RUMBLE TRIGGER               Set the attenuation (0-7) of rumble/haptic motors and trigger vibration
      trigger TRIGGER off                      remove all effects
      trigger TRIGGER feedback POSITION STRENGTH  set a resistance starting at position with a defined strength
      trigger TRIGGER weapon START STOP STRENGTH  Emulate weapon like gun trigger
      trigger TRIGGER bow START STOP STRENGTH SNAPFORCE    Emulate weapon like bow
      trigger TRIGGER galloping START STOP FIRST_FOOT SECOND_FOOT FREQUENCY    Emulate a galloping
      trigger TRIGGER machine START STOP STRENGTH_A STRENGTH_B FREQUENCY PERIOD    Switch vibration between to strength at a specified period
      trigger TRIGGER vibration POSITION AMPLITUDE FREQUENCY  Vibrates motor arm around specified position
      trigger TRIGGER feedback-raw STRENGTH[10]  set a resistance starting using array of strength
      trigger TRIGGER vibration-raw AMPLITUDE[10] FREQUENCY  Vibrates motor arm at position and strength specified by an array of amplitude
      trigger TRIGGER MODE [PARAMS]            set the trigger (left, right or both) mode with parameters (up to 9)
      monitor [add COMMAND] [remove COMMAND]   Run shell command COMMAND on add/remove events


AUR: [dualsensectl-git](https://aur.archlinux.org/packages/dualsensectl-git/)

### Dependencies

* libhidapi-hidraw
* libdbus-1
* libudev

### Building

    make && make install

### udev rules

Also installed by Steam, so you may already have it configured. If not, create `/etc/udev/rules.d/70-dualsensectl.rules`:

    # PS5 DualSense controller over USB hidraw
    KERNEL=="hidraw*", ATTRS{idVendor}=="054c", ATTRS{idProduct}=="0ce6", MODE="0660", TAG+="uaccess"

    # PS5 DualSense controller over bluetooth hidraw
    KERNEL=="hidraw*", KERNELS=="*054C:0CE6*", MODE="0660", TAG+="uaccess"

### Credit

the following docs:
 - https://controllers.fandom.com/wiki/Sony_DualSense/Data_Structures
 - https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db
