idf_component_register(
    SRCS "esp32-agent.cpp"
         "src/display/oled_display.cpp"
         "src/audio/audio_codec.cpp"
         "src/audio/wake_word.cpp"
         "src/led/rgb_led.cpp"
         "src/storage.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_lcd esp_timer lvgl spiffs esp-sr led_strip esp_codec_dev
)
