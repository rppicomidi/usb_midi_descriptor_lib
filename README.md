# usb_midi_descriptor_lib
Retrieve the string indices from a USB MIDI Device's interface descriptors

This library is designed to be used with TinyUSB. It can parse out the
MIDI interface descriptors from a USB MIDI device's configuration descriptor
or operate directly on the device's MIDI interface descriptors.The
information this library gets from a MIDI device's interface descriptors
that the TinyUSB `midi_host` driver does not already extract are the
string descriptors associated with the input and output jacks, which
are generally useful, and all other string descriptors in the MIDI device,
which are useful mainly for deep copy of a MIDI device in a filtering
application such as [pico-usb-midi-filter](https://github.com/rppicomidi/pico-usb-midi-filter)
and [pico-usb-midi-processor](https://github.com/rppicomidi/pico-usb-midi-processor).
