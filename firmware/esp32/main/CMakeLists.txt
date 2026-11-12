idf_component_register(
    SRCS "esp32-agent.cpp"
         "src/display/oled_display.cpp"
         "src/audio/speaker.cpp"
         "src/audio/mic.cpp"
         "src/storage.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_driver_i2s esp_lcd esp_timer lvgl spiffs
)
