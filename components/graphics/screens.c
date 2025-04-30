#include "screens.h"
#include "app_manager.h"

#include "conf.h"
#include "core/lv_obj.h"
#include "core/lv_obj_event.h"
#include "esp_log.h"
#include "fonts/impact_60.c"
#include "images/chicken_jockey.c"
#include "misc/lv_event.h"
#include <stdint.h>

screen_entry_t main_screens[MAIN_SCREEN_COUNT] = {0};
screen_entry_t *current_screen = NULL;

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
      .header.w = 480,
      .header.h = 480,
      .data_size = 480 * 480 * LV_COLOR_DEPTH / 8,
      .header.cf = LV_COLOR_FORMAT_RGB565, /*Set the color format*/
      .data = chicken_jockey_map,
  };

  lv_img_set_src(front_img, &chicken_jockey_dsc);
  lv_obj_center(front_img);

  initialize_lock_slider(lock_screen);

  return GFX_OK;
}

static void go_lockscreen_cb(lv_event_t *e) {
  (void)e;
  if (app_manager_open(NULL)) {
    emit_gfx_err();
  }

  if (set_active_screen_by_main_id(SCREEN_LOCK)) {
    emit_gfx_err();
  }
}

static void go_home_cb(lv_event_t *e) {
  (void)e;
  if (app_manager_open(NULL)) {
    emit_gfx_err();
  }

  if (set_active_screen_by_main_id(SCREEN_HOME)) {
    emit_gfx_err();
  }
}

static void go_settings_cb(lv_event_t *e) {
  (void)e;
  if (app_manager_open(NULL)) {
    emit_gfx_err();
  }

  if (set_active_screen_by_main_id(SCREEN_SETTINGS)) {
    emit_gfx_err();
  }
}

static lv_obj_t *home_time_label;

static void update_home_time_cb(lv_timer_t *timer) {
  (void)timer; // unused

  // TODO: Work on setting up timezoning with settings
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
  tzset();

  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  char buf[64];
  strftime(buf, sizeof(buf), "%H:%M:%S", t); // Format: HH:MM:SS

  lv_label_set_text(home_time_label, buf);
}

static void open_app_cb(lv_event_t *e) {
  lv_obj_t *app_btn = lv_event_get_target(e);

  int app_idx = (int)(uintptr_t)lv_obj_get_user_data(app_btn);
  app_t *app = NULL;
  if (app_idx >= 0 && app_idx < MAX_APPS) {
    app = apps[app_idx];
  }

  if (app == NULL) {
    return;
  }

  if (app_manager_open(app)) {
    emit_gfx_err();
  }
}

GFX_CALL init_home_screen(void) {
  lv_obj_t *home_screen = lv_obj_create(NULL);
  main_screens[SCREEN_HOME].root = home_screen;

  static lv_coord_t row_dsc[] = {NAVBAR_H, SCREEN_H - NAVBAR_H,
                                 LV_GRID_TEMPLATE_LAST};
  static lv_coord_t col_dsc[] = {LV_PCT(100), LV_GRID_TEMPLATE_LAST};
  lv_obj_t *grid = lv_obj_create(home_screen);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, SCREEN_W, SCREEN_H);
  lv_obj_center(grid);

  lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);

  // Navbar
  lv_obj_t *nav_bar = lv_obj_create(grid);
  lv_obj_remove_style_all(nav_bar);
  lv_obj_set_grid_cell(nav_bar, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_START, 0, 1);
  lv_obj_set_height(nav_bar, NAVBAR_H); // Set nav bar height
  lv_obj_set_style_bg_color(nav_bar, lv_color_hex(0x222222), 0); // Dark bar

  lv_obj_set_layout(nav_bar, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(nav_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(nav_bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_border_side(nav_bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(nav_bar, 3, 0);

  // -- Navbar -> Content
  static lv_style_t nav_text_style;
  lv_style_init(&nav_text_style);
  lv_style_set_text_font(&nav_text_style, &lv_font_montserrat_20);

  static lv_style_t nav_icon_style;
  lv_style_init(&nav_icon_style);
  lv_style_set_text_font(&nav_icon_style, &lv_font_montserrat_30);

  lv_obj_t *content[3]; // 0 left, 1 center, 2 right content
  uint16_t i;
  for (i = 0; i < 3; i++) {
    content[i] = lv_obj_create(nav_bar);
    lv_obj_remove_style_all(content[i]);
    lv_obj_set_layout(content[i], LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content[i], LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content[i], LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(content[i], 8, 0);
    lv_obj_set_style_pad_all(content[i], 8, 0);
  }

  // -- Navbar -- Content -> Left Content
  lv_obj_t *home_btn = lv_button_create(content[0]);
  lv_obj_remove_style_all(home_btn); // Remove all default styles
  lv_obj_set_size(home_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(home_btn, 8, 0);
  lv_obj_set_style_bg_opa(home_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(home_btn, lv_color_hex(0xDDDDDD), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(home_btn, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_add_event_cb(home_btn, go_lockscreen_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *home_btn_lbl = lv_label_create(home_btn);
  lv_obj_add_style(home_btn_lbl, &nav_icon_style, 0);
  lv_label_set_text(home_btn_lbl, LV_SYMBOL_HOME);
  lv_obj_center(home_btn_lbl);

  lv_obj_t *wifi_icon = lv_label_create(content[0]);
  lv_obj_add_style(wifi_icon, &nav_icon_style, 0);
  lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);

  // -- Navbar -- Content -> Center Content
  home_time_label = lv_label_create(content[1]);
  lv_obj_add_style(home_time_label, &nav_text_style, 0);
  lv_timer_create(update_home_time_cb, 1000, NULL);

  // -- Navbar -- Content -> Right Content
  lv_obj_t *battery_icon = lv_label_create(content[2]);
  lv_obj_add_style(battery_icon, &nav_icon_style, 0);
  lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_3);

  lv_obj_t *settings_btn = lv_button_create(content[2]);
  lv_obj_remove_style_all(settings_btn); // Remove all default styles
  lv_obj_set_size(settings_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(settings_btn, 8, 0);
  lv_obj_set_style_bg_opa(settings_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(settings_btn, lv_color_hex(0xDDDDDD),
                            LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(settings_btn, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_add_event_cb(settings_btn, go_settings_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *settings_btn_lbl = lv_label_create(settings_btn);
  lv_obj_add_style(settings_btn_lbl, &nav_icon_style, 0);
  lv_label_set_text(settings_btn_lbl, LV_SYMBOL_SETTINGS);
  lv_obj_center(settings_btn_lbl);

  // Apps
  lv_obj_t *app_area = lv_obj_create(grid);
  lv_obj_remove_style_all(app_area);
  lv_obj_set_grid_cell(app_area, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);

  lv_obj_set_layout(app_area, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(app_area, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(app_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  static lv_style_t app_text_style;
  lv_style_init(&app_text_style);
  lv_style_set_text_font(&app_text_style, &lv_font_montserrat_28);

  // -- Apps -> Content
  for (i = 0; i < MAX_APPS; i++) {
    app_t *app = apps[i];
    if (app == NULL)
      continue;

    lv_obj_t *app_btn = lv_btn_create(app_area);

    lv_obj_set_user_data(
        app_btn, (void *)(uintptr_t)i); // give the button the index to the app

    lv_obj_remove_style_all(app_btn);
    lv_obj_set_size(app_btn, SCREEN_W, 100); // App button size
    lv_obj_set_style_pad_all(app_btn, 8, 0);
    lv_obj_set_style_bg_opa(app_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(app_btn, lv_color_hex(0xDDDDDD),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(app_btn, LV_OPA_COVER, LV_STATE_PRESSED);

    lv_obj_t *app_btn_lbl = lv_label_create(app_btn);
    lv_obj_add_style(app_btn_lbl, &app_text_style, 0);
    lv_label_set_text_fmt(app_btn_lbl, "%s    %s", app->icon, app->name);
    lv_obj_align(app_btn_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_add_event_cb(app_btn, open_app_cb, LV_EVENT_CLICKED, NULL);
  }

  return GFX_OK;
}

GFX_CALL init_settings_screen(void) {
  lv_obj_t *settings_screen = lv_obj_create(NULL);
  main_screens[SCREEN_SETTINGS].root = settings_screen;

  lv_obj_t *label = lv_label_create(settings_screen);
  lv_label_set_text(label, "Settings");
  lv_obj_center(label);

  lv_obj_t *go_btn = lv_btn_create(settings_screen);
  lv_obj_align(go_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_size(go_btn, 120, 50);
  lv_obj_add_event_cb(go_btn, go_home_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *go_btn_lbl = lv_label_create(go_btn);
  lv_label_set_text(go_btn_lbl, "Return");
  lv_obj_center(go_btn_lbl);

  return GFX_OK;
}

void register_main_screens(void) {
  main_screens[SCREEN_LOCK].init_screen = init_lock_screen;
  main_screens[SCREEN_LOCK].idx = SCREEN_LOCK;
  main_screens[SCREEN_HOME].init_screen = init_home_screen;
  main_screens[SCREEN_HOME].idx = SCREEN_HOME;
  main_screens[SCREEN_SETTINGS].init_screen = init_settings_screen;
  main_screens[SCREEN_SETTINGS].idx = SCREEN_SETTINGS;
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
  if (screen_entry == NULL || screen_entry->root == NULL) {
    append_error("%s", "Invalid Screen Entry in set_active_screen()");
    return GFX_ERR;
  }

  // initial screen
  if (current_screen == NULL) {
    current_screen = screen_entry;
    lv_scr_load(current_screen->root);
    return GFX_OK;
  }

  // TODO: apps here

  // direct left?
  if (current_screen->idx + 1 == screen_entry->idx) {
    lv_scr_load_anim(screen_entry->root, LV_SCR_LOAD_ANIM_MOVE_LEFT, 400, 0,
                     false);
    current_screen = screen_entry;
    return GFX_OK;
  }

  // direct right?
  if (current_screen->idx - 1 == screen_entry->idx) {
    lv_scr_load_anim(screen_entry->root, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 400, 0,
                     false);
    current_screen = screen_entry;
    return GFX_OK;
  }

  current_screen = screen_entry;
  lv_scr_load_anim(screen_entry->root, LV_SCR_LOAD_ANIM_FADE_IN, 400, 0, false);
  return GFX_OK;
}

GFX_CALL init_screens(void) {
  register_main_screens();
  initialize_fonts();

  // init main screens
  int i = 0;
  for (i = 0; i < MAIN_SCREEN_COUNT; i++) {
    if (main_screens[i].init_screen) {
      if (main_screens[i].init_screen()) {
        return GFX_ERR;
      }
    }
  }

  if (set_active_screen_by_main_id(SCREEN_LOCK)) {
    return GFX_ERR;
  }

  return GFX_OK;
}
