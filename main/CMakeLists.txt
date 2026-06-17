idf_component_register(SRCS "app_main.cpp"
                            "app_display.cpp"
                            "app_font.cpp"
                            "app_input.cpp"
                            "app_ui.cpp"
                            "app_snapshot.cpp"
                            "text_input_composer.cpp"
                            "keybinding.cpp"
                            "text_view.cpp"
                            "piece_table.cpp"
                            "undo_stack.cpp"
                            "editor_core.cpp"
                            "x12y12pxMaruMinyaHangul_36.c"
                    INCLUDE_DIRS "."
                    REQUIRES tab5_keyboard
                             esp_driver_usb_serial_jtag
                             esp_timer
                             esp_psram
                             esp_partition
                             nvs_flash
                             lvgl__lvgl
                             espressif__m5stack_tab5
                             espressif__usb
                             espressif__usb_host_hid)
