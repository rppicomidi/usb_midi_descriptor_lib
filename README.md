# usb_midi_descriptor_lib
Retrieve the string indices from a USB MIDI Device's interface descriptors

This library is designed to be used with TinyUSB. It can parse out the
MIDI interface descriptors from a USB MIDI device's configuration descriptor
or operate directly on the device's MIDI interface descriptors. The
information this library gets from a MIDI device's interface descriptors
that the TinyUSB `midi_host` driver does not already extract are the
string descriptors associated with the input and output jacks, which
are generally useful, and all other string descriptors in the MIDI device,
which are useful mainly for deep copy of a MIDI device in a filtering
application such as [pico-usb-midi-filter](https://github.com/rppicomidi/pico-usb-midi-filter)
and [pico-usb-midi-processor](https://github.com/rppicomidi/pico-usb-midi-processor).

# Example code
The `examples` directory contains example software that demonstrates the
use of this library with the `midi_host` software. The example software
is designed to run on a Raspberry Pi Pico board. However, with minor
modification, it should run on any target hardware that TinyUSB provides
with USB Host support.

Each example is designed to test the USB MIDI Host driver for a single USB
MIDI device connected to the USB Host port or up to `CFG_TUH_MIDI` devices connected
to the USB Host port through a USB hub. Upon connection of a MIDI device,
the demo software uses the `usb_midi_descriptor_lib_configure()` function to initialize
the library using the data from the `midi_host` `tuh_midi_descriptor_cb()` function.
Once the MIDI device is fully mounted, the example software sends to the serial port
console the MIDI device's Manufacturer String, Product String, Serial String, and
the Virtual MIDI IN and MIDI OUT cable ID strings gleaned from the MIDI Device's Device
Descriptor, MIDI Interface Descriptors, and String Descriptors.

While the demo software runs, it periodically sends to each USB MIDI device the
sequence of half-steps from B-flat to D whose note numbers correspond to the
transport button LEDs on a Mackie Control compatible control surface. It also
prints to a serial port console the messages received from each USB MIDI device.
