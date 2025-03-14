cmake_minimum_required(VERSION 3.13)

add_library(usb_midi_descriptor_lib INTERFACE)
target_sources(usb_midi_descriptor_lib INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/usb_midi_descriptor_lib.c
)
target_include_directories(usb_midi_descriptor_lib INTERFACE
 ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(usb_midi_descriptor_lib INTERFACE pico_stdlib tinyusb_host)