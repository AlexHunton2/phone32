#include "graphics.h"
#include "waveshare_rgb_lcd_port.h"

#include "fonts/impact_60.c"
#include "images/chicken_jockey.c"

// error handling:
char gfx_err[128] = {0};
screen_entry_t main_screens[MAIN_SCREEN_COUNT] = {0};
screen_entry_t *current_screen = NULL;

void append_error(const char *fmt, ...) {
  size_t len = strlen(gfx_err);
  if (len >= sizeof(gfx_err) - 1) {
    return; // Buffer is full, don't append
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(gfx_err + len, sizeof(gfx_err) - len, fmt, args);
  va_end(args);
}

const char *get_gfx_err(void) { return gfx_err; }

void emit_gfx_err(void) { ESP_LOGI(GRAPHICS_TAG, "%s", gfx_err); }

static void slider_set_value_anim_cb(void *obj, int32_t v) {
  lv_slider_set_value((lv_obj_t *)obj, v, LV_ANIM_OFF);
}

static void lock_slider_event_cb(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target(e);
  int value = lv_slider_get_value(slider);

  // Only act when the user releases the slider
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
    if (value <= 95) {
      // Animate back to 0
      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, slider);
      lv_anim_set_exec_cb(&a, slider_set_value_anim_cb);
      lv_anim_set_time(&a, 300); // 300 ms
      lv_anim_set_values(&a, value, 0);
      lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
      lv_anim_start(&a);
    } else {
      lv_slider_set_value(slider, 0, LV_ANIM_ON);
      set_active_screen_by_main_id(SCREEN_HOME);
    }
  }
}

void initialize_lock_slider(lv_obj_t *parent) {
  lv_obj_t *slider = lv_slider_create(parent);
  lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_slider_set_range(slider, 0, 100);
  lv_obj_set_size(slider, 360, 40);

  // Style the main slider (track)
  static lv_style_t style_main;
  lv_style_init(&style_main);
  lv_style_set_bg_color(&style_main, lv_color_hex(0xFBCAEF));
  lv_style_set_bg_opa(&style_main, LV_OPA_COVER);
  lv_obj_add_style(slider, &style_main, LV_PART_MAIN);

  static lv_style_t style_indic;
  lv_style_init(&style_indic);
  lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
  lv_style_set_bg_color(&style_indic, lv_color_hex(0xF865B0));
  lv_obj_add_style(slider, &style_indic, LV_PART_INDICATOR);

  // Style the knob
  static lv_style_t style_knob;
  lv_style_init(&style_knob);
  lv_style_set_bg_color(&style_knob, lv_color_hex(0xF865B0));
  lv_obj_add_style(slider, &style_knob, LV_PART_KNOB);

  lv_obj_set_style_pad_ver(slider, -2, LV_PART_KNOB | LV_STATE_PRESSED);

  lv_obj_add_event_cb(slider, lock_slider_event_cb, LV_EVENT_ALL, NULL);
}

GFX_CALL init_lock_screen(void) {
  lv_obj_t *lock_screen = lv_obj_create(NULL);
  main_screens[SCREEN_LOCK].root = lock_screen;

  lv_obj_t *title_lbl = lv_label_create(lock_screen);
  lv_obj_add_style(title_lbl, &style_impact_60, 0);
  lv_label_set_text(title_lbl, "PHONE32");
  lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 40);

  lv_obj_t *front_img = lv_img_create(lock_screen);

  static lv_img_dsc_t chicken_jockey_dsc = {
      .header.always_zero = 0,
      .header.w = 480,
      .header.h = 480,
      .data_size = 480 * 480 * LV_COLOR_DEPTH / 8,
      .header.cf = LV_IMG_CF_TRUE_COLOR, /*Set the color format*/
      .data = chicken_jockey_map,
  };

  lv_img_set_src(front_img, &chicken_jockey_dsc);
  lv_obj_center(front_img);

  initialize_lock_slider(lock_screen);

  return GFX_OK;
}

static void go_btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    set_active_screen_by_main_id(SCREEN_LOCK);
  }
}

GFX_CALL init_home_screen(void) {
  lv_obj_t *home_screen = lv_obj_create(NULL);
  main_screens[SCREEN_HOME].root = home_screen;

  lv_obj_t *label = lv_label_create(home_screen);
  lv_label_set_text(label, "Home Screen");
  lv_obj_center(label);

  lv_obj_t *go_btn = lv_btn_create(home_screen);
  lv_obj_align(go_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_size(go_btn, 120, 50);
  lv_obj_add_event_cb(go_btn, go_btn_event_cb, LV_EVENT_ALL, NULL);
  lv_obj_t *go_btn_lbl = lv_label_create(go_btn);
  lv_label_set_text(go_btn_lbl, "Button");
  lv_obj_center(go_btn_lbl);

  return GFX_OK;
}

void register_main_screens(void) {
  main_screens[SCREEN_LOCK].init_screen = init_lock_screen;
  main_screens[SCREEN_LOCK].idx = SCREEN_LOCK;
  main_screens[SCREEN_HOME].init_screen = init_home_screen;
  main_screens[SCREEN_HOME].idx = SCREEN_HOME;
}

void initialize_fonts(void) {
  lv_style_init(&style_impact_60);
  lv_style_set_text_font(&style_impact_60, &impact_60);
}

GFX_CALL set_active_screen_by_main_id(main_screen_id_t id) {
  if (id < MAIN_SCREEN_COUNT && main_screens[id].root != NULL) {
    set_active_screen(&main_screens[id]);
    return GFX_OK;
  }

  append_error("%s", "Failed to set active screen in set_active_by_main_id()");
  return GFX_ERR;
}

GFX_CALL set_active_screen(screen_entry_t *screen_entry) {
  if (screen_entry == NULL || screen_entry->idx == -1) {
    append_error("%s", "Invalid Screen Entry in set_active_screen()");
    return GFX_ERR;
  }

  // initial screen
  if (current_screen == NULL) {
    current_screen = screen_entry;
    lv_scr_load(current_screen->root);
    return GFX_OK;
  }

  // no transition for same to same
  if (current_screen->idx == screen_entry->idx) {
    current_screen = screen_entry;
    lv_scr_load(current_screen->root);
    return GFX_OK;
  }

  if (current_screen->idx < MAIN_SCREEN_COUNT &&
      screen_entry->idx < MAIN_SCREEN_COUNT) {
    // transition main screen to main screen (left to right or right to left)
    if (current_screen->idx < screen_entry->idx) {
      lv_scr_load_anim(screen_entry->root, LV_SCR_LOAD_ANIM_MOVE_LEFT, 400, 0,
                       false);
    } else {
      lv_scr_load_anim(screen_entry->root, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 400, 0,
                       false);
    }
  } else {
    // TODO :Figure out transitions for app to app / main to app / app to main
    current_screen = screen_entry;
    lv_scr_load(current_screen->root);
    return GFX_OK;
  }

  current_screen = screen_entry;
  return GFX_OK;
}

GFX_CALL graphics_init(void) {
  waveshare_esp32_s3_rgb_lcd_init();

  if (lvgl_port_lock(-1)) {
    ESP_LOGI(WAVESHARE_TAG, "Loading Phone32 Graphics...");
    register_main_screens();
    initialize_fonts();

    // init main screens
    int i = 0;
    for (i = 0; i < MAIN_SCREEN_COUNT; i++) {
      if (main_screens[i].init_screen != NULL) {
        main_screens[i].init_screen();
      }
    }

    if (set_active_screen_by_main_id(SCREEN_LOCK)) {
      emit_gfx_err();
    }

    lvgl_port_unlock();
  }

  return GFX_OK;
}
