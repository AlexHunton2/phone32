idf_component_register(SRCS "graphics.c" "drivers.c" "lvglport.c"
                  INCLUDE_DIRS "."
                  REQUIRES lvgl esp_lcd_touch_gt911
)

idf_component_get_property(lvgl_lib lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
