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

#include "usb_midi_descriptor_lib.h"
#include "pico/multicore.h"

typedef struct
{
  bool configured;
  uint8_t ep_in;          // IN endpoint address
  uint8_t ep_out;         // OUT endpoint address
  uint8_t num_cables_rx;  // IN endpoint CS descriptor bNumEmbMIDIJack value
  uint8_t num_cables_tx;  // OUT endpoint CS descriptor bNumEmbMIDIJack value
  uint8_t all_string_indices[MAX_STRING_INDICES];
  uint8_t num_string_indices;
  struct {
    uint8_t jack_id;
    uint8_t jack_type;
    uint8_t string_index;
  } in_jack_info[MAX_IN_JACKS];
  uint8_t next_in_jack;
  struct {
    uint8_t jack_id;
    uint8_t jack_type;
    uint8_t num_source_ids;
    uint8_t source_ids[MAX_IN_JACKS];
    uint8_t string_index;
  } out_jack_info[MAX_OUT_JACKS];
  uint8_t next_out_jack;
  uint8_t ep_in_associated_jacks[MAX_IN_CABLES];
  uint8_t ep_out_associated_jacks[MAX_OUT_CABLES];
} usb_midi_descriptor_info_t;

// This descriptor follows the standard bulk data endpoint descriptor
typedef struct
{
  uint8_t bLength            ; ///< Size of this descriptor in bytes (4+bNumEmbMIDIJack)
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be CS_ENDPOINT
  uint8_t bDescriptorSubType ; ///< Descriptor SubType, must be MS_GENERAL
  uint8_t bNumEmbMIDIJack;   ; ///< Number of embedded MIDI jacks associated with this endpoint
  uint8_t baAssocJackID[];   ; ///< A list of associated jacks
} midi_cs_desc_endpoint_t;

static usb_midi_descriptor_info_t midi_host;

void usb_midi_descriptor_lib_init()
{
  memset(&midi_host, 0, sizeof(midi_host));
}

bool usb_midi_descriptor_lib_configure_from_full(uint8_t* full_config_descriptor)
{
  tusb_desc_interface_t const *desc_itf = (tusb_desc_interface_t*)(tu_desc_next(full_config_descriptor));
  uint16_t max_len = ((tusb_desc_configuration_t*)full_config_descriptor)->wTotalLength - tu_desc_len(full_config_descriptor);
  midi_host.num_string_indices = 0;
  uint16_t len_parsed = 0;
  while (len_parsed < max_len && TUSB_CLASS_AUDIO != desc_itf->bInterfaceClass)
  {
    len_parsed += tu_desc_len(desc_itf);
    desc_itf = (tusb_desc_interface_t*)tu_desc_next(desc_itf);
  }
  TU_VERIFY(len_parsed < max_len && TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass);
  TU_LOG2("Full interface descriptor:\r\n");
  TU_LOG_MEM(2, desc_itf, max_len, 2);
  // There can be just a MIDI interface or an audio and a MIDI interface. Only open the MIDI interface
  uint8_t const *p_desc = (uint8_t const *) desc_itf;
  if (AUDIO_SUBCLASS_CONTROL == desc_itf->bInterfaceSubClass)
  {
    // Keep track of any string descriptor that might be here
    if (desc_itf->iInterface != 0)
      midi_host.all_string_indices[midi_host.num_string_indices++] = desc_itf->iInterface;
    // If this is the audio control interface there might be a MIDI interface following it.
    // Search through every descriptor until a MIDI interface is found or the end of the descriptor is found
    while (len_parsed < max_len && (desc_itf->bInterfaceClass != TUSB_CLASS_AUDIO || desc_itf->bInterfaceSubClass != AUDIO_SUBCLASS_MIDI_STREAMING))
    {
      len_parsed += desc_itf->bLength;
      p_desc = tu_desc_next(p_desc);
      desc_itf = (tusb_desc_interface_t const *)p_desc;
    }

    TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass);
  }
  TU_VERIFY(AUDIO_SUBCLASS_MIDI_STREAMING == desc_itf->bInterfaceSubClass);
  return usb_midi_descriptor_lib_configure(p_desc, max_len - len_parsed);
}

bool usb_midi_descriptor_lib_configure(uint8_t const *midi_descriptor, uint32_t max_len)
{
  tusb_desc_interface_t const *desc_itf = (tusb_desc_interface_t*)(midi_descriptor);
  // Keep track of any string descriptor that might be here
  if (desc_itf->iInterface != 0)
      midi_host.all_string_indices[midi_host.num_string_indices++] = desc_itf->iInterface;
  uint32_t len_parsed = desc_itf->bLength;
  uint8_t const *p_desc = tu_desc_next(midi_descriptor);
  // Find out if getting the MIDI class specific interface header or an endpoint descriptor
  // or a class-specific endpoint descriptor
  // Jack descriptors or element descriptors must follow the cs interface header,
  // but this driver does not support devices that contain element descriptors

  // assume it is an interface header
  midi_desc_header_t const *p_mdh = (midi_desc_header_t const *)p_desc;
  TU_VERIFY((p_mdh->bDescriptorType == TUSB_DESC_CS_INTERFACE && p_mdh->bDescriptorSubType == MIDI_CS_INTERFACE_HEADER) || 
    (p_mdh->bDescriptorType == TUSB_DESC_CS_ENDPOINT && p_mdh->bDescriptorSubType == MIDI_CS_ENDPOINT_GENERAL) ||
    p_mdh->bDescriptorType == TUSB_DESC_ENDPOINT);

  uint8_t prev_ep_addr = 0; // the CS endpoint descriptor is associated with the previous endpoint descrptor
  while (len_parsed < max_len)
  {
    TU_VERIFY((p_mdh->bDescriptorType == TUSB_DESC_CS_INTERFACE) || 
      (p_mdh->bDescriptorType == TUSB_DESC_CS_ENDPOINT && p_mdh->bDescriptorSubType == MIDI_CS_ENDPOINT_GENERAL) ||
      p_mdh->bDescriptorType == TUSB_DESC_ENDPOINT);

    if (p_mdh->bDescriptorType == TUSB_DESC_CS_INTERFACE) {
      // The USB host doesn't really need this information unless it uses
      // the string descriptor for a jack or Element

      // assume it is an input jack
      midi_desc_in_jack_t const *p_mdij = (midi_desc_in_jack_t const *)p_desc;
      if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_HEADER)
      {
        TU_LOG2("Found MIDI Interface Header\r\n");
      }
      else if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_IN_JACK)
      {
        // Then it is an in jack. 
        TU_LOG2("Found in jack %u\r\n", p_mdij->bJackID);
        if (midi_host.next_in_jack < MAX_IN_JACKS)
        {
          midi_host.in_jack_info[midi_host.next_in_jack].jack_id = p_mdij->bJackID;
          midi_host.in_jack_info[midi_host.next_in_jack].jack_type = p_mdij->bJackType;
          midi_host.in_jack_info[midi_host.next_in_jack].string_index = p_mdij->iJack;
          ++midi_host.next_in_jack;
          // Keep track of any string descriptor that might be here
          if (p_mdij->iJack != 0)
            midi_host.all_string_indices[midi_host.num_string_indices++] = p_mdij->iJack;
        }
      }
      else if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_OUT_JACK)
      {
        // then it is an out jack
        TU_LOG2("Found out jack %u\r\n", p_mdij->bJackID);
        if (midi_host.next_out_jack < MAX_OUT_JACKS)
        {
          midi_desc_out_jack_t const *p_mdoj = (midi_desc_out_jack_t const *)p_desc;
          midi_host.out_jack_info[midi_host.next_out_jack].jack_id = p_mdoj->bJackID;
          midi_host.out_jack_info[midi_host.next_out_jack].jack_type = p_mdoj->bJackType;
          midi_host.out_jack_info[midi_host.next_out_jack].num_source_ids = p_mdoj->bNrInputPins;
          const struct associated_jack_s {
              uint8_t id;
              uint8_t pin;
          } *associated_jack = (const struct associated_jack_s *)(p_desc+6);
          int jack;
          for (jack = 0; jack < p_mdoj->bNrInputPins; jack++)
          {
            midi_host.out_jack_info[midi_host.next_out_jack].source_ids[jack] = associated_jack->id;
          }
          midi_host.out_jack_info[midi_host.next_out_jack].string_index = *(p_desc+6+p_mdoj->bNrInputPins*2);
          ++midi_host.next_out_jack;
          if (p_mdoj->iJack != 0)
            midi_host.all_string_indices[midi_host.num_string_indices++] = p_mdoj->iJack;
        }
      }
      else if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_ELEMENT)
      {
        // the it is an element; collect its string index if there is one
        const uint8_t* element_descriptor = (const uint8_t*)p_mdij;
        uint8_t idx= element_descriptor[element_descriptor[0]-1];
        if (idx > 0)
          midi_host.all_string_indices[midi_host.num_string_indices++] = idx;
        TU_LOG2("Found element\r\n");
      }
      else
      {
        TU_LOG2("Unknown CS Interface sub-type %u\r\n", p_mdij->bDescriptorSubType);
        TU_VERIFY(false); // unknown CS Interface sub-type
      }
      len_parsed += p_mdij->bLength;
    }
    else if (p_mdh->bDescriptorType == TUSB_DESC_CS_ENDPOINT)
    {
      TU_LOG2("found CS_ENDPOINT Descriptor for %02x\r\n", prev_ep_addr);
      TU_VERIFY(prev_ep_addr != 0);
      // parse out the mapping between the device's embedded jacks and the endpoints
      // Each embedded IN jack is assocated with an OUT endpoint
      midi_cs_desc_endpoint_t const* p_csep = (midi_cs_desc_endpoint_t const*)p_mdh;
      if (tu_edpt_dir(prev_ep_addr) == TUSB_DIR_OUT)
      {
        TU_VERIFY(midi_host.ep_out == prev_ep_addr);
        TU_VERIFY(midi_host.num_cables_tx == 0);
        midi_host.num_cables_tx = p_csep->bNumEmbMIDIJack;
        uint8_t jack;
        uint8_t max_jack = midi_host.num_cables_tx;
        if (max_jack > sizeof(midi_host.ep_out_associated_jacks))
        {
            max_jack = sizeof(midi_host.ep_out_associated_jacks);
        }
        for (jack = 0; jack < max_jack; jack++)
        {
          midi_host.ep_out_associated_jacks[jack] = p_csep->baAssocJackID[jack];
        }
      }
      else
      {
        TU_VERIFY(midi_host.ep_in == prev_ep_addr);
        TU_VERIFY(midi_host.num_cables_rx == 0);
        midi_host.num_cables_rx = p_csep->bNumEmbMIDIJack;
        uint8_t jack;
        uint8_t max_jack = midi_host.num_cables_rx;
        if (max_jack > sizeof(midi_host.ep_in_associated_jacks))
        {
            max_jack = sizeof(midi_host.ep_in_associated_jacks);
        }
        for (jack = 0; jack < max_jack; jack++)
        {
          midi_host.ep_in_associated_jacks[jack] = p_csep->baAssocJackID[jack];
        }
      }
      len_parsed += p_csep->bLength;
      prev_ep_addr = 0;
    }
    else if (p_mdh->bDescriptorType == TUSB_DESC_ENDPOINT) {
      // parse out the bulk endpoint info
      tusb_desc_endpoint_t *p_ep = (tusb_desc_endpoint_t *)p_mdh;
      TU_LOG2("found ENDPOINT Descriptor %02x\r\n", p_ep->bEndpointAddress);
      if (tu_edpt_dir(p_ep->bEndpointAddress) == TUSB_DIR_OUT)
      {
        TU_VERIFY(midi_host.ep_out == 0);
        TU_VERIFY(midi_host.num_cables_tx == 0);
        midi_host.ep_out = p_ep->bEndpointAddress;
        prev_ep_addr = midi_host.ep_out;
      }
      else
      {
        TU_VERIFY(midi_host.ep_in == 0);
        TU_VERIFY(midi_host.num_cables_rx == 0);
        midi_host.ep_in = p_ep->bEndpointAddress;
        prev_ep_addr = midi_host.ep_in;
      }
      len_parsed += p_mdh->bLength;
    }
    p_desc = tu_desc_next(p_desc);
    p_mdh = (midi_desc_header_t const *)p_desc;
  }
  TU_LOG2("ep_out=%u num_cables_tx=%u ep_in=%u num_cables_rx=%u\r\n",midi_host.ep_out, midi_host.num_cables_tx, midi_host.ep_in, midi_host.num_cables_rx);
  TU_VERIFY((midi_host.ep_out != 0 && midi_host.num_cables_tx != 0) ||
            (midi_host.ep_in != 0 && midi_host.num_cables_rx != 0));
  TU_LOG2("MIDI descriptor parsed successfully\r\n");
  // remove duplicate string indices
  for (int idx=0; idx < midi_host.num_string_indices; idx++) {
      for (int jdx = idx+1; jdx < midi_host.num_string_indices; jdx++) {
          while (jdx < midi_host.num_string_indices && midi_host.all_string_indices[idx] == midi_host.all_string_indices[jdx]) {
              // delete the duplicate by overwriting it with the last entry and reducing the number of entries by 1
              midi_host.all_string_indices[jdx] = midi_host.all_string_indices[midi_host.num_string_indices-1];
              --midi_host.num_string_indices;
          }
      }
  }
  midi_host.configured = true;
  TU_LOG2("MIDI String descriptors parsed successfully\r\n");
  return true;
}

int usb_midi_descriptor_lib_get_all_str_inidices(const uint8_t** inidices)
{
  uint8_t nstrings = -1;
  if (midi_host.configured)
  {
    nstrings = midi_host.num_string_indices;
    if (nstrings)
      *inidices = midi_host.all_string_indices;
  }
  return nstrings;
}

int usb_midi_descriptor_lib_get_str_idx_for_in_cable(uint8_t in_cable_num)
{
  if (in_cable_num >= midi_host.num_cables_rx)
    return 0;
  uint8_t jack_id = midi_host.ep_in_associated_jacks[in_cable_num];
  // The jacks associated with an IN endpoint will be embedded OUT jacks
  for (uint8_t jack_idx = 0; jack_idx < midi_host.next_out_jack; jack_idx++)
  {
    if (midi_host.out_jack_info[jack_idx].jack_id == jack_id) {
      return midi_host.out_jack_info[jack_idx].string_index;
    }
  }
  return 0;
}

int usb_midi_descriptor_lib_get_str_idx_for_out_cable(uint8_t out_cable_num)
{
  if (out_cable_num >= midi_host.num_cables_tx)
    return 0;
  uint8_t jack_id = midi_host.ep_out_associated_jacks[out_cable_num];
  // The jacks associated with an OUT endpoint will be embedded IN jacks
  for (uint8_t jack_idx = 0; jack_idx < midi_host.next_in_jack; jack_idx++)
  {
    if (midi_host.in_jack_info[jack_idx].jack_id == jack_id) {
      return midi_host.in_jack_info[jack_idx].string_index;
    }
  }
  return 0;
}
