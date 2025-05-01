#include "drivers.h"
#include "esp_err.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_types.h"
#include "esp_log.h"

static const char *DRIVERS_TAG = "drivers";

void test_lcd_red_rectangle(esp_lcd_panel_handle_t *panel_handle) {
  // Allocate memory to PSRAM with DMA
  uint16_t *framebuffer =
      heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
                       MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
  if (framebuffer == NULL) {
    ESP_LOGE("FB", "Framebuffer allocation failed!");
    return;
  }

  // Fill white background
  for (int y = 0; y < LCD_V_RES; y++) {
    for (int x = 0; x < LCD_H_RES; x++) {
      framebuffer[y * LCD_H_RES + x] = 0xFFFF;
    }
  }

  // Draw red rectangle
  for (int y = 60; y < 140; y++) {
    for (int x = 100; x < 200; x++) {
      framebuffer[y * LCD_H_RES + x] = 0xF800;
    }
  }

  // Refresh framebuffer
  // ESP_ERROR_CHECK(esp_lcd_rgb_panel_refresh(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(*panel_handle, 0, 0, LCD_H_RES,
                                            LCD_V_RES, framebuffer));
}

void init_lcd_driver(esp_lcd_panel_handle_t *lcd_handle) {
  ESP_LOGI(DRIVERS_TAG, "Initialize LCD Driver");

  esp_lcd_rgb_panel_config_t panel_config = {
      .clk_src = LCD_CLK_SRC_DEFAULT, // Set the clock source for the panel
      .timings =
          {
              .pclk_hz = LCD_PIXEL_CLOCK_HZ, // Pixel clock frequency
              .h_res = LCD_H_RES,            // Horizontal resolution
              .v_res = LCD_V_RES,            // Vertical resolution
              .hsync_pulse_width = HSYNC_PULSE_WIDTH,
              .hsync_back_porch = HSYNC_BACK_PORCH,   // Horizontal back porch
              .hsync_front_porch = HSYNC_FRONT_PORCH, // Horizontal front porch
              .vsync_pulse_width =
                  VSYNC_PULSE_WIDTH,                // Vertical sync pulse width
              .vsync_back_porch = VSYNC_BACK_PORCH, // Vertical back porch
              .vsync_front_porch = VSYNC_FRONT_PORCH, // Vertical front porch
              .flags =
                  {
                      .pclk_active_neg = true, // Active low pixel clock
                  },
          },
      .data_width = RGB_DATA_WIDTH,        // Data width for RGB
      .bits_per_pixel = RGB_BIT_PER_PIXEL, // Bits per pixel
      .num_fbs = LCD_RGB_BUFFER_NUMS,      // Number of frame buffers
      //.bounce_buffer_size_px =
      //    RGB_BOUNCE_BUFFER_SIZE,         // Bounce buffer size in pixels
      .hsync_gpio_num = LCD_IO_RGB_HSYNC, // GPIO number for horizontal sync
      .vsync_gpio_num = LCD_IO_RGB_VSYNC, // GPIO number for vertical sync
      .de_gpio_num = LCD_IO_RGB_DE,       // GPIO number for data enable
      .pclk_gpio_num = LCD_IO_RGB_PCLK,   // GPIO number for pixel clock
      .disp_gpio_num = LCD_IO_RGB_DISP,   // GPIO number for display
      .data_gpio_nums =
          {
              LCD_IO_RGB_DATA0,
              LCD_IO_RGB_DATA1,
              LCD_IO_RGB_DATA2,
              LCD_IO_RGB_DATA3,
              LCD_IO_RGB_DATA4,
              LCD_IO_RGB_DATA5,
              LCD_IO_RGB_DATA6,
              LCD_IO_RGB_DATA7,
              LCD_IO_RGB_DATA8,
              LCD_IO_RGB_DATA9,
              LCD_IO_RGB_DATA10,
              LCD_IO_RGB_DATA11,
              LCD_IO_RGB_DATA12,
              LCD_IO_RGB_DATA13,
              LCD_IO_RGB_DATA14,
              LCD_IO_RGB_DATA15,
          },
      .flags =
          {
              .fb_in_psram = 1, // Use PSRAM for framebuffer
          },
  };

  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, lcd_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(*lcd_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(*lcd_handle));

  esp_lcd_panel_swap_xy(*lcd_handle, true);
  esp_lcd_panel_mirror(*lcd_handle, false, true);
}

// Reset the touch screen
void touch_reset() {
  uint8_t write_buf = 0x01;
  i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1,
                             I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

  // Reset the touch screen. It is recommended to reset the touch screen before
  // using it.
  write_buf = 0x2C;
  i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1,
                             I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  esp_rom_delay_us(100 * 1000);
  gpio_set_level(GPIO_INPUT_IO_4, 0);
  esp_rom_delay_us(100 * 1000);
  write_buf = 0x2E;
  i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1,
                             I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  esp_rom_delay_us(200 * 1000);
}

void init_touch_driver(esp_lcd_touch_handle_t *tp_handle) {
  ESP_LOGI(DRIVERS_TAG, "Initialize Touch Driver");

  ESP_LOGI(DRIVERS_TAG,
           "Initialize I2C bus"); // Log the initialization of the I2C bus

  int i2c_master_port = I2C_MASTER_NUM;

  i2c_config_t i2c_conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };

  // Configure I2C parameters
  i2c_param_config(i2c_master_port, &i2c_conf);

  // Install I2C driver
  ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, i2c_conf.mode, 0, 0, 0));

  ESP_LOGI(DRIVERS_TAG, "Initialize GPIO"); // Log GPIO initialization
  // Zero-initialize the config structure
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;     // Disable interrupt
  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL; // Bit mask of the pins
  io_conf.mode = GPIO_MODE_OUTPUT;           // Input Mode

  gpio_config(&io_conf);
  ESP_LOGI(DRIVERS_TAG, "Initialize Touch LCD"); // Log touch LCD initialization
  touch_reset();                                 // Reset the touch panel

  esp_lcd_panel_io_handle_t tp_io_handle =
      NULL; // Declare a handle for touch panel I/O
  const esp_lcd_panel_io_i2c_config_t tp_io_config =
      ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG(); // Configure I2C for GT911 touch
                                           // controller

  ESP_LOGI(DRIVERS_TAG,
           "Initialize I2C panel IO"); // Log I2C panel I/O initialization
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(
      (esp_lcd_i2c_bus_handle_t)I2C_MASTER_NUM, &tp_io_config,
      &tp_io_handle)); // Create new I2C panel I/O

  ESP_LOGI(DRIVERS_TAG,
           "Initialize touch controller GT911"); // Log touch controller
                                                 // initialization
  const esp_lcd_touch_config_t tp_cfg = {
      .x_max = LCD_H_RES,                // Set maximum X coordinate
      .y_max = LCD_V_RES,                // Set maximum Y coordinate
      .rst_gpio_num = PIN_NUM_TOUCH_RST, // GPIO number for reset
      .int_gpio_num = PIN_NUM_TOUCH_INT, // GPIO number for interrupt
      .levels =
          {
              .reset = 0,     // Reset level
              .interrupt = 0, // Interrupt level
          },
      .flags =
          {
              .swap_xy = 1,  // Yes swap of X and Y
              .mirror_x = 0, // No mirroring of X
              .mirror_y = 1, // Yes mirroring of Y
          },
  };

  ESP_ERROR_CHECK(
      esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, tp_handle));
}
