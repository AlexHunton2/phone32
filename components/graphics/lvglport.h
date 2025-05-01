#ifndef _LVGLPORT_H_
#define _LVGLPORT_H_

#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_types.h"
#include "esp_log.h"

#include "lvgl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_H_RES 800
#define LCD_V_RES 480

#define TIMER_TASK_MAX_DELAY_MS 500
#define TIMER_TASK_MIN_DELAY_MS 10

typedef struct lcd_driver_data {
  esp_lcd_panel_handle_t lcd_handle;
} lcd_driver_data_t;

typedef struct touch_driver_data {
  esp_lcd_touch_handle_t tp_handle;
} touch_driver_data_t;

void lvport_timer_handler_task(void *pvParameters);
void lvport_connect_display_interface(esp_lcd_panel_handle_t *lcd_handle);
void lvport_connect_touch_interface(esp_lcd_touch_handle_t *tp_handle);

bool lvport_lock(int timeout_ms);
void lvport_unlock(void);

#endif
