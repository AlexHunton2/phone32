#ifndef _DRIVERS_H_
#define _DRIVERS_H_

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

#define LCD_H_RES 800
#define LCD_V_RES 480

// Not enough ram in internal memory for the LCD screen we got so need to store
// it in PSRAM

// these values are found in datasheet for ST7262
// https://files.waveshare.com/wiki/common/ST7262.pdf
#define LCD_PIXEL_CLOCK_HZ (13 * 1000 * 1000)
#define HSYNC_PULSE_WIDTH (4)
#define HSYNC_BACK_PORCH (8)
#define HSYNC_FRONT_PORCH (8)
#define VSYNC_PULSE_WIDTH (4)
#define VSYNC_BACK_PORCH (8)
#define VSYNC_FRONT_PORCH (8)

#define RGB_DATA_WIDTH (16)
#define RGB_BIT_PER_PIXEL (16)

// Bounce Buffers (not necessary), but breaks up all the display data into
// pieces to make the display more efficient.
#define RGB_BOUNCE_BUFFER_SIZE                                                 \
  (LCD_V_RES * 40 *                                                            \
   2) // H_RES pixels per line, 8 lines in the bounce, 2 bytes per pixel

#define LCD_RGB_BUFFER_NUMS (2)

#define LCD_IO_RGB_DISP (-1) // -1 if not used
#define LCD_IO_RGB_VSYNC (GPIO_NUM_3)
#define LCD_IO_RGB_HSYNC (GPIO_NUM_46)
#define LCD_IO_RGB_DE (GPIO_NUM_5)
#define LCD_IO_RGB_PCLK (GPIO_NUM_7)
#define LCD_IO_RGB_DATA0 (GPIO_NUM_14)
#define LCD_IO_RGB_DATA1 (GPIO_NUM_38)
#define LCD_IO_RGB_DATA2 (GPIO_NUM_18)
#define LCD_IO_RGB_DATA3 (GPIO_NUM_17)
#define LCD_IO_RGB_DATA4 (GPIO_NUM_10)
#define LCD_IO_RGB_DATA5 (GPIO_NUM_39)
#define LCD_IO_RGB_DATA6 (GPIO_NUM_0)
#define LCD_IO_RGB_DATA7 (GPIO_NUM_45)
#define LCD_IO_RGB_DATA8 (GPIO_NUM_48)
#define LCD_IO_RGB_DATA9 (GPIO_NUM_47)
#define LCD_IO_RGB_DATA10 (GPIO_NUM_21)
#define LCD_IO_RGB_DATA11 (GPIO_NUM_1)
#define LCD_IO_RGB_DATA12 (GPIO_NUM_2)
#define LCD_IO_RGB_DATA13 (GPIO_NUM_42)
#define LCD_IO_RGB_DATA14 (GPIO_NUM_41)
#define LCD_IO_RGB_DATA15 (GPIO_NUM_40)

// TOUCH
#define I2C_MASTER_NUM                                                         \
  0 // I2C master i2c port number, the number of i2c peripheral interfaces
    // available will depend on the chip
#define I2C_MASTER_TIMEOUT_MS 1000
#define I2C_MASTER_SCL_IO 9       // GPIO number used for I2C master clock
#define I2C_MASTER_SDA_IO 8       // GPIO number used for I2C master data
#define I2C_MASTER_FREQ_HZ 400000 // I2C master clock frequency

#define GPIO_INPUT_IO_4 4
#define GPIO_INPUT_PIN_SEL 1ULL << GPIO_INPUT_IO_4

#define PIN_NUM_TOUCH_RST (-1) // -1 if not used
#define PIN_NUM_TOUCH_INT (-1) // -1 if not used

void init_lcd_driver(esp_lcd_panel_handle_t *lcd_handle);
void init_touch_driver(esp_lcd_touch_handle_t *tp_handle);

#endif
