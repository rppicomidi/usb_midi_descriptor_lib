/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 rppicomidi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/*
 * This library extracts all the string indices from a USB MIDI device's
 * interface descriptors and provides an API for retrieving them.
 * It assumes that there is at most one IN endpoint and/or one OUT endpoint
 * for the MIDI device. If the device has two or more MIDI IN endpoints
 * or two or more MIDI OUT endpoints, then this library will fail.
 */


#pragma once
#include <stdint.h>
#include "tusb.h"
#ifndef MAX_STRING_INDICES
#define MAX_STRING_INDICES 40
#endif
#ifndef MAX_IN_JACKS
#define MAX_IN_JACKS 16
#endif

#ifndef MAX_OUT_JACKS
#define MAX_OUT_JACKS 16
#endif

#ifndef MAX_IN_CABLES
#define MAX_IN_CABLES 16
#endif

#ifndef MAX_OUT_CABLES
#define MAX_OUT_CABLES 16
#endif

/**
 * @brief Initialize data structures for parsing a new MIDI descriptor
 */
void usb_midi_descriptor_lib_init(uint8_t idx);

/**
 * @brief Parse the full configuration descriptor to discover the MIDI device's string indices
 * 
 * @param full_config_descriptor The device's full configuration descriptor
 * @return true if a MIDI descriptor was found in the full configuration descriptor
 * and the string indicies were successfully parsed
 */
bool usb_midi_descriptor_lib_configure_from_full(uint8_t idx, const uint8_t* full_config_descriptor);

/**
 * @brief Parse the MIDI Interface descriptor to disconver the MIDI device's string indices
 * 
 * @param midi_descriptor a pointer to the first byte of the MIDI descriptor
 * @param max_len The number of bytes in the MIDI descriptor
 * @return true if the string indicies were successfully parsed
 */
bool usb_midi_descriptor_lib_configure(uint8_t idx, uint8_t const *midi_descriptor, uint32_t max_len);

/**
 * @brief set indices to point to an array of all MIDI interface string indices
 * 
 * @param inidices a pointer to an array of string indices
 * @return int the number of indices in the array
 */
int usb_midi_descriptor_lib_get_all_str_inidices(uint8_t idx, const uint8_t** inidices);

/**
 * @brief Get the string index for a particular MIDI IN virtual cable
 * 
 * @param in_cable_num the cable number, 0-15
 * @return int the string index or 0 if none is found
 */
int usb_midi_descriptor_lib_get_str_idx_for_in_cable(uint8_t idx, uint8_t in_cable_num);

/**
 * @brief Get the string index for a particular MIDI OUT virtual cable
 * 
 * @param out_cable_num the cable number, 0-15
 * @return int the string index or 0 if none is found
 */
int usb_midi_descriptor_lib_get_str_idx_for_out_cable(uint8_t idx, uint8_t out_cable_num);