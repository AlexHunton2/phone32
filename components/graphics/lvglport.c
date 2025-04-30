#include "lvglport.h"
#include <stdint.h>

static const char *TAG = "LVGL_PORT";
static SemaphoreHandle_t lvgl_mux;

void lvport_timer_handler_task(void *pvParameters) {
  ESP_LOGD(TAG, "Starting LVGL task"); // Log the task start

  uint32_t task_delay_ms = TIMER_TASK_MAX_DELAY_MS; // Set initial task delay
  while (1) {
    if (lvport_lock(-1)) {                // Try to lock the LVGL mutex
      task_delay_ms = lv_timer_handler(); // Handle LVGL timer events
      lvport_unlock();                    // Unlock the mute
    }
    // Ensure the delay time is within limits
    if (task_delay_ms > TIMER_TASK_MAX_DELAY_MS) {
      task_delay_ms = TIMER_TASK_MAX_DELAY_MS;
    } else if (task_delay_ms < TIMER_TASK_MIN_DELAY_MS) {
      task_delay_ms = TIMER_TASK_MIN_DELAY_MS;
    }
    vTaskDelay(
        pdMS_TO_TICKS(task_delay_ms)); // Delay the task for the calculated time
  }
}

void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
  const int offsetx1 = area->x1; // Start X coordinate of the area to flush
  const int offsetx2 = area->x2; // End X coordinate of the area to flush
  const int offsety1 = area->y1; // Start Y coordinate of the area to flush
  const int offsety2 = area->y2; // End Y coordinate of the area to flush

  lcd_driver_data_t *driver_data = lv_display_get_driver_data(display);
  assert(driver_data->lcd_handle);

  /* Switch the current RGB frame buffer to `color_map` */
  esp_lcd_panel_draw_bitmap(driver_data->lcd_handle, offsetx1, offsety1,
                            offsetx2 + 1, offsety2 + 1, px_map);

  /* Inform LVGL that flushing is complete so buffer can be modified again. */
  lv_display_flush_ready(display);
}

void lvport_connect_display_interface(esp_lcd_panel_handle_t *lcd_handle) {
  assert(lcd_handle); // Ensure the panel handle is valid

  ESP_LOGI(TAG, "Connecting Display Interface");

  lv_display_t *display = lv_display_create(LCD_V_RES, LCD_H_RES);

  lcd_driver_data_t *driver_data =
      malloc(sizeof(lcd_driver_data_t)); // TODO: Free this lol
  driver_data->lcd_handle = *lcd_handle;
  lv_display_set_driver_data(display, (void *)driver_data);

  uint32_t buffer_size = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
  uint16_t *buf1 =
      heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
  uint16_t *buf2 =
      heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);

  if (buf1 == NULL || buf2 == NULL) {
    ESP_LOGE("FB", "Framebuffer allocation failed in PSRAM!");
    return;
  }

  /*
  // Use Internal Buffers, 40 lines of H_RES
  uint32_t buffer_size = 40 * LCD_H_RES * sizeof(uint16_t);
  uint16_t *buf1 =
      heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  uint16_t *buf2 =
      heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  if (buf1 == NULL || buf2 == NULL) {
    ESP_LOGE("FB", "Framebuffer allocation in internal memory!");
    return;
  }
  */
  lvgl_mux = xSemaphoreCreateRecursiveMutex();

  lv_display_set_buffers(display, buf1, buf2, buffer_size,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_display_set_flush_cb(display, flush_cb);
}

void input_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  touch_driver_data_t *driver_data = lv_indev_get_driver_data(indev);
  assert(driver_data->tp_handle);

  uint16_t touchpad_x;
  uint16_t touchpad_y;
  uint8_t touchpad_cnt = 0;

  esp_lcd_touch_read_data(driver_data->tp_handle);

  bool touchpad_pressed = esp_lcd_touch_get_coordinates(
      driver_data->tp_handle, &touchpad_x, &touchpad_y, NULL, &touchpad_cnt, 1);
  if (touchpad_pressed && touchpad_cnt > 0) {
    data->point.x = touchpad_x;
    data->point.y = touchpad_y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void lvport_connect_touch_interface(esp_lcd_touch_handle_t *tp_handle) {
  lv_indev_t *indev = lv_indev_create();

  touch_driver_data_t *driver_data =
      malloc(sizeof(lcd_driver_data_t)); // TODO: Free this lol
  driver_data->tp_handle = *tp_handle;
  lv_indev_set_driver_data(indev, driver_data);

  lv_indev_set_read_cb(indev, input_read_cb);
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
}

bool lvport_lock(int timeout_ms) {
  assert(lvgl_mux);
  const TickType_t timeout_ticks =
      (timeout_ms < 0) ? portMAX_DELAY
                       : pdMS_TO_TICKS(timeout_ms); // Convert timeout to ticks
  return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) ==
         pdTRUE; // Try to take the mutex
}

void lvport_unlock(void) {
  assert(lvgl_mux);
  xSemaphoreGiveRecursive(lvgl_mux); // Release the mutex
}
