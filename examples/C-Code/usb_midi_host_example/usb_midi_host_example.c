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

/**
 * This demo program is designed to test the USB MIDI Host driver for a single USB
 * MIDI device connected to the USB Host port or up to CFG_TUH_MIDI devices connected
 * to the USB Host port through a USB hub. It sends to each USB MIDI device the
 * sequence of half-steps from B-flat to D whose note numbers correspond to the
 * transport button LEDs on a Mackie Control compatible control surface. It also
 * prints to a UART serial port console the messages received from each USB MIDI device.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_midi_descriptor_lib.h"
#include "utf16_to_utf8.h"
#ifdef RASPBERRYPI_PICO_W
// The Board LED is controlled by the CYW43 WiFi/Bluetooth module
#include "pico/cyw43_arch.h"
#endif

static uint8_t midi_dev_idx[CFG_TUH_MIDI];
static bool display_dev_strings[CFG_TUH_MIDI];

static void blink_led(void)
{
    static absolute_time_t previous_timestamp = {0};

    static bool led_state = false;

    absolute_time_t now = get_absolute_time();
    
    int64_t diff = absolute_time_diff_us(previous_timestamp, now);
    if (diff > 1000000) {
#ifdef RASPBERRYPI_PICO_W
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
#else
        board_led_write(led_state);
#endif
        led_state = !led_state;
        previous_timestamp = now;
    }
}

static void send_next_note(void)
{
    static uint8_t first_note = 0x5b; // Mackie Control rewind
    static uint8_t last_note = 0x5f; // Mackie Control stop
    static uint8_t message[6] = {
        0x90, 0x5f, 0x00,
        0x90, 0x5b, 0x7f,
    };
    // toggle NOTE On, Note Off for the Mackie Control channels 1-8 REC LED
    const uint32_t interval_ms = 1000;
    static uint32_t start_ms = 0;

    // Blink every interval ms
    if ( board_millis() - start_ms < interval_ms) {
        return; // not enough time
    }
    start_ms += interval_ms;

    for (uint8_t idx = 0; idx < CFG_TUH_MIDI; idx++) {
        if (!tuh_midi_mounted(midi_dev_idx[idx]))
            continue;
        // Transmit the note message on the highest cable number
        uint8_t cable = tuh_midi_get_tx_cable_count(midi_dev_idx[idx]) - 1;
        uint32_t nwritten = tuh_midi_stream_write(midi_dev_idx[idx], cable, message, sizeof(message));
        if (nwritten != sizeof(message)) {
            return;
        }
    }
    ++message[1];
    ++message[4];
    if (message[1] > last_note)
        message[1] = first_note;
    if (message[4] > last_note)
        message[4] = first_note;
}

static void print_string_descriptor(uint16_t *buffer)
{
    const size_t maxsrc = ((*buffer) & 0xFF)/2 - 1;
    const size_t maxdest = maxsrc*2+1; // allows up to 2 bytes to encode each utf-16 code unit
    uint8_t dest[maxdest];
    utf16ToUtf8(buffer+1, maxsrc, dest, maxdest);
    printf("%s\r\n", dest);
}

int main() {

    bi_decl(bi_program_description("A USB MIDI host example."));
    memset(midi_dev_idx, TUSB_INDEX_INVALID_8, sizeof(midi_dev_idx));
    memset(display_dev_strings, 0, sizeof(display_dev_strings));
    for (uint8_t idx = 0; idx < CFG_TUH_MIDI; idx++)
        usb_midi_descriptor_lib_init(idx);
 
    board_init();

    printf("Pico MIDI Host Example\r\n");
    tusb_init();

#ifdef RASPBERRYPI_PICO_W
    // for the LED blink
    if (cyw43_arch_init()) {
        printf("WiFi/Bluetooh module init for LED blink failed");
        return -1;
    }
#endif
    while (1) {
        tuh_task();

        blink_led();

        send_next_note();
        // Flush all pending transmit data
        for (uint8_t idx=0; idx < CFG_TUH_MIDI; idx++) {
            if (tuh_midi_mounted(midi_dev_idx[idx]) && tuh_midi_get_tx_cable_count(midi_dev_idx[idx] > 0)) {
                tuh_midi_write_flush(midi_dev_idx[idx]);
            }
            if (display_dev_strings[idx]) {
                tuh_itf_info_t info;
                uint16_t buffer[128];
                if (tuh_midi_itf_get_info(midi_dev_idx[idx], &info)) {
                    if (tuh_descriptor_get_string_langid_sync(info.daddr, buffer, sizeof(buffer)) == XFER_RESULT_SUCCESS) {
                        display_dev_strings[idx] = false;
                        uint16_t langid = buffer[1];
                        printf("For device %u at address %u:\r\n", idx, info.daddr);
                        if (tuh_descriptor_get_manufacturer_string_sync(info.daddr, langid, buffer, sizeof(buffer))== XFER_RESULT_SUCCESS) {
                            printf("manufacturer: ");
                            print_string_descriptor(buffer);
                        }
                        if (tuh_descriptor_get_product_string_sync(info.daddr, langid, buffer, sizeof(buffer))== XFER_RESULT_SUCCESS) {
                            printf("product: ");
                            print_string_descriptor(buffer);
                        }
                        if (tuh_descriptor_get_serial_string_sync(info.daddr, langid, buffer, sizeof(buffer))== XFER_RESULT_SUCCESS) {
                            printf("serial: ");
                            print_string_descriptor(buffer);
                        }
                        for (uint jdx = 0; jdx < tuh_midi_get_rx_cable_count(midi_dev_idx[idx]); jdx++) {
                            uint8_t desc_idx = usb_midi_descriptor_lib_get_str_idx_for_in_cable(idx, jdx);
                            if (desc_idx > 0) {
                                if (tuh_descriptor_get_string_sync(info.daddr, desc_idx, langid, buffer, sizeof(buffer))== XFER_RESULT_SUCCESS) {
                                    printf("USB MIDI IN cable 0: ");
                                    print_string_descriptor(buffer);
                                }
                            }
                        }
                        for (uint jdx = 0; jdx < tuh_midi_get_tx_cable_count(midi_dev_idx[idx]); jdx++) {
                            uint8_t desc_idx = usb_midi_descriptor_lib_get_str_idx_for_out_cable(idx, jdx);
                            if (desc_idx > 0) {
                                if (tuh_descriptor_get_string_sync(info.daddr, desc_idx, langid, buffer, sizeof(buffer))== XFER_RESULT_SUCCESS) {
                                    printf("USB MIDI OUT cable 0: ");
                                    print_string_descriptor(buffer);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+
void tuh_midi_descriptor_cb(uint8_t idx, const tuh_midi_descriptor_cb_t * desc_cb_data)
{
    usb_midi_descriptor_lib_configure(idx, (uint8_t const *)(desc_cb_data->desc_midi), desc_cb_data->desc_midi_total_len);
}

void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t* mount_cb_data)
{
  tuh_itf_info_t info;
  tuh_midi_itf_get_info(idx, &info);
  printf("MIDI device %u address = %u, IN endpoint has %u cables, OUT endpoint has %u cables\r\n",
      idx, info.daddr, mount_cb_data->rx_cable_count, mount_cb_data->tx_cable_count);
  midi_dev_idx[idx] = idx;
  display_dev_strings[idx] = true;
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t idx)
{
    tuh_itf_info_t info;
    tuh_midi_itf_get_info(idx, &info);
    usb_midi_descriptor_lib_init(idx);

    midi_dev_idx[idx] = TUSB_INDEX_INVALID_8;
    display_dev_strings[idx] = false;
    printf("MIDI device %u address %u is unmounted\r\n", idx, info.daddr);
}

void tuh_midi_rx_cb(uint8_t idx, uint32_t xferred_bytes)
{
  if (tuh_midi_mounted(idx)) {
    if (xferred_bytes != 0) {
      uint8_t cable_num;
      uint8_t buffer[48];
      while (1) {
        uint32_t bytes_read = tuh_midi_stream_read(idx, &cable_num, buffer, sizeof(buffer));
        if (bytes_read == 0)
          return;
        printf("Dev %u Cable #%u:", idx, cable_num);
        for (uint32_t jdx = 0; jdx < bytes_read; jdx++) {
          printf("%02x ", buffer[jdx]);
        }
        printf("\r\n");
      }
    }
  }
}

void tuh_midi_tx_cb(uint8_t idx, uint32_t xferred_bytes)
{
    (void)xferred_bytes;
    (void)idx;
}
