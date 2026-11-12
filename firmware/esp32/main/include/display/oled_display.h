#ifndef CAR_BUDDY_OLED_DISPLAY_H_
#define CAR_BUDDY_OLED_DISPLAY_H_

#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <lvgl.h>

class OledDisplay {
 public:
  OledDisplay() = default;
  ~OledDisplay();

  OledDisplay(const OledDisplay&) = delete;
  OledDisplay& operator=(const OledDisplay&) = delete;

  // Initializes I2C, the SSD1306 panel, and LVGL. Call once from app_main.
  void init();

  // Replaces the whole screen with a single centered line of text.
  void showText(const char* text);

 private:
  static bool onColorTransDone(esp_lcd_panel_io_handle_t io,
                               esp_lcd_panel_io_event_data_t* edata,
                               void* user_ctx);
  static void flushCallback(lv_display_t* disp, const lv_area_t* area,
                            uint8_t* px_map);

  void initI2c();
  void initPanel();

  i2c_master_bus_handle_t i2c_bus_ = nullptr;
  esp_lcd_panel_io_handle_t io_handle_ = nullptr;
  esp_lcd_panel_handle_t panel_handle_ = nullptr;
  lv_display_t* display_ = nullptr;
  lv_obj_t* label_ = nullptr;
};

#endif
