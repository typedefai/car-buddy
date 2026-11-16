idf_component_register(
    SRCS "esp32-agent.cpp"
         "src/display/oled_display.cpp"
         "src/audio/audio_codec.cpp"
         "src/audio/wake_word.cpp"
         "src/link/stm32_link.cpp"
         "src/link/drive_console.cpp"
         "src/led/rgb_led.cpp"
         "src/storage.cpp"
         "../../../shared/protocol/car_protocol.c"
    INCLUDE_DIRS "include" "../../../shared/protocol"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_uart esp_lcd esp_timer lvgl spiffs esp-sr led_strip esp_codec_dev
)
