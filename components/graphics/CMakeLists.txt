idf_component_register(SRCS "graphics.c" "drivers.c" "lvglport.c" "app_manager.c" "app.c" "screens.c" "conf.c" "apps/test_app.c" "apps/suspend_app.c" "apps/background_app.c" "apps/phone_app.c"
                  INCLUDE_DIRS "." "apps/."
                  REQUIRES lvgl esp_lcd_touch_gt911
)

idf_component_get_property(lvgl_lib lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
