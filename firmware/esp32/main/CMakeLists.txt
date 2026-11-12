idf_component_register(
    SRCS "esp32-agent.cpp"
         "src/display/oled_display.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_lcd esp_timer lvgl
)
