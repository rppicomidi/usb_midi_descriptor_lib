# usb_midi_descriptor_lib
Retrieve the string indices from a USB MIDI Device's interface descriptors

This library is designed to be used with TinyUSB's `midi_host` driver.
It can parse out the MIDI interface descriptors from a USB MIDI device's
configuration descriptor or operate directly on the device's MIDI
interface descriptors that `midi_host` returns on enumeration. The data
this library gets from a MIDI device's interface descriptors that the
TinyUSB `midi_host` driver does not already extract are the MIDI
interface's string descriptors. The strings descriptors associated with
the Input Jacks and Output Jacks can provide useful labels on
a user interface. The other string descriptors are useful mainly for deep
copy of a MIDI device in a filtering application such as [pico-usb-midi-filter](https://github.com/rppicomidi/pico-usb-midi-filter)
and [pico-usb-midi-processor](https://github.com/rppicomidi/pico-usb-midi-processor).

If you want this library to provide an API to access other information
described in the USB MIDI string descriptors, please file a feature
request issue.

Also in this library is a C-language header file that contains the
function `utf16ToUtf8()` for converting a UTF-16LE string descriptor into
a UTF-8 formatted NULL terminated C string. I have only tested this routine
for US English strings, but if I read the Unicode specification
correctly, it will work for character sets for other languages. Please
file issues if you find conversion errors.

# Example code

## Functional Description
The `examples` directory contains example software that demonstrates the
use of this library with the `midi_host` software. The example software
is designed to run on a Raspberry Pi Pico board. However, with minor
modification, it should run on any target hardware that TinyUSB provides
with USB Host support.

Each example is designed to test the USB MIDI Host driver and this library
for a single USB MIDI device connected to the USB Host port or up to
`CFG_TUH_MIDI` devices connected to the USB Host port through a USB
hub. Upon connection of a MIDI device, the demo software uses the
`usb_midi_descriptor_lib_configure()` function to initialize the library
using the data from the `midi_host` `tuh_midi_descriptor_cb()` function.
Once the MIDI device is fully mounted, the example software sends to the
serial port console the MIDI device's Manufacturer String, Product
String, Serial String, and the Virtual MIDI IN and MIDI OUT cable ID
strings gleaned from the MIDI Device's Device Descriptor, MIDI Interface
Descriptors, and String Descriptors. The console will also report when a
MIDI device was unplugged.

While the demo software runs, it periodically sends to each USB MIDI
device the sequence of half-steps from B-flat to D whose note numbers
correspond to the transport button LEDs on a Mackie Control compatible
control surface. It also prints to a serial port console the messages
received from each USB MIDI device.

## Hardware
### For Raspberry Pi Pico platform native USB hardware
The Raspberry Pi Pico and similar boards have a USB connector that is
normally used for a USB Device interface. To adapt that hardware for
a host, the simplest thing to do is to plug in a USB OTG adapter
and provide 5VDC to the board's VBUS pin (Pin 40 on a Raspberry Pi Pico).
Then plug the MIDI device or a USB hub to the USB OTG connector.
That 5VDC will power the target board and will also power the devices
connected to the host, so please make sure the 5V supply is capable of
powering both the board and the connected devices. Also, please be
sure the 5VDC has the correct polarity and voltage or else you may
damage the Pico board and whatever device you plug to the OTG connector.
When I am bringing up hardware, I always connect my low cost USB hub
to the OTG connector's USB A plug first to make sure the hardware works
before plugging my expensive USB MIDI gear to the hub.

### For Raspberry Pi Pico platform PIO USB hardware
You can use the RP2040 or RP2350 PIO state machines along with some
software to make a USB host port. Wiring this requires only wiring
the DP and DM pins of a USB A connector to 2 GPIO pins and providing
5V and Ground to the other two USB A connector pins. The DP pin is
wired to GPIO16 and the DM pin is wired to GPIO17. On a Pico board,
this is pins 21 and 22. On a Pico board, you can get 5VDC from pin 40
and the GND pin from pin 23. These GPIO pins were chosen because the [Adafruit Feather RP2040 with USB A Host](https://learn.adafruit.com/adafruit-feather-rp2040-with-usb-type-a-host)
board uses the same GPIO pins for DP and DM. This code will therefore
also run on the Adafruit board with no modificaiton.

Note that at the time of this writing, the Pico PIO USB based hardware
has issues with MIDI device unplug from a USB hub. Depending on timing,
unplugging a MIDI device from a hub can lock up the USB system. The
workaround i    s to unplug the hub, then unplug the device, then plug the
hub back in.

## Build instructions

### C-Code (for Raspberry Pi Pico SDK)
To build a C-code example, please first make sure the `pico-sdk` uses a
TinyUSB version from 20-March-2025 or later. As of this writing,
the latest Raspberry Pi `pico-sdk`, version, 2.1.1, has an older TinyUSB.
If you are using the The Official Rasberry Pi VS Code Extension as
described in the [Getting Started with the Raspberry Pi Pico Series](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)
document, then you will find the TinyUSB library at `${HOME}/.pico-sdk/sdk/[version number]/lib/tinyusb`, where `[version number]` is `2.1.1`
or later. If you are building from the command line, the TinyUSB library
is stored at `${PICO_SDK_PATH}/lib/tinyusb`. To update TinyUSB in the
`pico-sdk` to the latest version, execute these commands:
```
cd [the path to TinyUSB as described above]
git checkout master
git pull
```
If your hardware uses PIO and GPIO for the USB host port, then you
need to install the source code for the [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB)
project, too. TinyUSB wants you to run a script to install this. You
must have python installed to run this script
```
cd [the path to TinyUSB as described above]
python tools/get_deps.py rp2040
```
Alternatively, you can just clone the latest version of the
Pico-PIO-USB project directly to your TinyUSB source tree
```
cd [the path to TinyUSB as described above]/hw/mcu/raspberrypi
git clone https://github.com/sekigon-gonnoc/Pico-PIO-USB.git
```

Once you have an up-to-date TinyUSB version in the `pico-sdk`, clone
the project code.
```
cd [some project directory]
git clone https://github.com/rppicomidi/usb_midi_descriptor_lib.git
```
The project code will be in `[some project directory]/usb_midi_descriptor_lib`.
The C-Code example directories are found in
`[some project directory]/usb_midi_descriptor_lib/examples/C-Code/`

To build using VS Code and the Raspberry Pi Pico Extension, click the
import icon and navigate to subdirectory of the C-Code example directory
that contains the project you want to build and import the project
normally. It should import without issue. Then build and run as usual.

To build using the command line,
```
export PICO_SDK_PATH=[the path to the pico-sdk directory]
#if your board is is not a pico, uncomment and execute the next line
#export PICO_BOARD=[your board name, such as pico2]
cd [some project directory]/usb_midi_descriptor_lib/examples/C-Code/[example directory name]
mkdir build
cd build
cmake ..
make
```
### Arduino
This library should be fully usable with Arduino once TinyUSB ports
the `midi_host` driver to the Adafruit TinyUSB for Arduino library.
In the meantime, please use the [usb_midi_host](https://github.com/rppicomidi/usb_midi_host) TinyUSB application host driver library
instead of this library. Instructions for that library can be found
at the previous link.