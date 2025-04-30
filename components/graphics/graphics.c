#include "graphics.h"
#include "app_manager.h"
#include "apps.h"

static void app_loop_task(void *arg) {
  while (1) {
    if (lvport_lock(-1)) {
      app_manager_loop();
    }
    lvport_unlock();
    vTaskDelay(pdMS_TO_TICKS(APP_LOOP_DELAY_MS));
  }
}

GFX_CALL graphics_init(void) {
  // 1. Initialzie LVGL
  lv_init();

  // 2. Initialize Drivers
  esp_lcd_panel_handle_t lcd_handle = NULL;
  esp_lcd_touch_handle_t tp_handle = NULL;

  init_lcd_driver(&lcd_handle);
  init_touch_driver(&tp_handle);

  // 3. Connect Tick Interface
  lv_tick_set_cb(xTaskGetTickCount);

  // 4. Connect Display Interface
  lvport_connect_display_interface(&lcd_handle);

  // 5. Connect Touch Interface
  lvport_connect_touch_interface(&tp_handle);

  // 6. Timer Handler Setup
  xTaskCreatePinnedToCore(lvport_timer_handler_task, "LVGL Task", 1024 * 24,
                          NULL, 2, NULL, 1);

  if (lvport_lock(-1)) {
    ESP_LOGI(GRAPHICS_TAG, "Loading Phone32 Graphics...");

    app_manager_init();
    app_manager_register_app(&test_app, NULL);
    app_manager_register_app(&suspend_app, NULL);
    app_manager_register_app(&background_app, NULL);
    app_manager_register_app(&phone_app, NULL);

    if (init_screens()) {
      emit_gfx_err();
      lvport_unlock();
      return GFX_ERR;
    }
    lvport_unlock();
  }

  xTaskCreatePinnedToCore(app_loop_task, "AppLoopTask",
                          1024 * 8, // Stack size
                          NULL,     // Parameter
                          5,        // Priority (adjust as needed)
                          NULL,     // Task handle (optional)
                          1 // Core to pin to (typically 0 or 1 on ESP32-S3)
  );

  return GFX_OK;
}
