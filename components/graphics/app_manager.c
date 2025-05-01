#include "app_manager.h"
#include "app.h"
#include "conf.h"
#include "esp_log.h"
#include "screens.h"

GFX_CALL app_manager_init() {
  current_app_idx = -1;
  return GFX_OK;
}

GFX_CALL app_manager_suspend(app_t *app) {
  if (app && app->on_suspend && app->app_screen)
    app->on_suspend(app->app_screen);
  app->status = SUSPENDED;

  return GFX_OK;
}

GFX_CALL app_manager_background(app_t *app) {
  if (app && app->on_background && app->app_screen)
    app->on_background(app->app_screen);
  app->status = BACKGROUND;
  return GFX_OK;
}

GFX_CALL app_manager_kill(app_t *app) {
  if (app) {
    app->status = KILLED;
  }
  return GFX_OK;
}

GFX_CALL app_manager_register_app(app_t *app, int *idx) {
  int index = -1;
  for (int i = 0; i < MAX_APPS; ++i) {
    if (apps[i] == app) {
      index = i;
      break;
    }
  }

  if (index == -1) {
    // Not found, add it
    for (int i = 0; i < MAX_APPS; ++i) {
      if (apps[i] == NULL) {
        apps[i] = app;
        index = i;
        break;
      }
    }
  }

  if (index == -1) {
    append_error("%s", "There isn't enough space to register another app.");
    return GFX_ERR;
  }

  if (idx != NULL) {
    *idx = index;
  }
  return GFX_OK;
}

static void go_homescreen_cb(lv_event_t *e) {
  (void)e;
  if (app_manager_open(NULL)) {
    emit_gfx_err();
  }

  if (set_active_screen_by_main_id(SCREEN_HOME)) {
    emit_gfx_err();
  }
}

void init_app_screen(app_t *app) {
  if (app == NULL)
    return;

  lv_obj_t *app_entire_screen = lv_obj_create(NULL);

  app->app_screen_entry.idx = -1;
  app->app_screen_entry.root = app_entire_screen;

  static lv_coord_t row_dsc[] = {NAVBAR_H, SCREEN_H - NAVBAR_H,
                                 LV_GRID_TEMPLATE_LAST};
  static lv_coord_t col_dsc[] = {LV_PCT(100), LV_GRID_TEMPLATE_LAST};
  lv_obj_t *grid = lv_obj_create(app_entire_screen);
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
  lv_obj_t *back_btn = lv_button_create(content[0]);
  lv_obj_remove_style_all(back_btn); // Remove all default styles
  lv_obj_set_size(back_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(back_btn, 8, 0);
  lv_obj_set_style_bg_opa(back_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xDDDDDD), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_add_event_cb(back_btn, go_homescreen_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *back_btn_lbl = lv_label_create(back_btn);
  lv_obj_add_style(back_btn_lbl, &nav_icon_style, 0);
  lv_label_set_text(back_btn_lbl, LV_SYMBOL_NEW_LINE);
  lv_obj_center(back_btn_lbl);

  // The actual part of the screen the app gets to work off of (720 x 480)
  lv_obj_t *app_area = lv_obj_create(grid);
  lv_obj_set_grid_cell(app_area, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);
  app->app_screen = app_area;
}

GFX_CALL app_manager_open(app_t *app) {
  // Put previous current app on ice
  if (current_app_idx != -1 && apps[current_app_idx]) {
    app_t *curr_app = apps[current_app_idx];
    if (curr_app->should_suspend_on_close) {
      app_manager_suspend(curr_app);
    } else {
      app_manager_background(curr_app);
    }
  }

  // go back to main screens, no app running
  if (app == NULL) {
    current_app_idx = -1;
    return GFX_OK;
  }

  if (app && app->status == KILLED) {
    append_error("%s", "Requested app to be opened has been marked for death.");
    return GFX_ERR;
  }

  int index = -1;
  if (app_manager_register_app(app, &index)) {
    return GFX_ERR;
  }

  // Start/Resume App
  if (!app->has_init && app->on_init) {
    init_app_screen(app);
    app->on_init(app->app_screen);
    app->has_init = true;

  } else if (app->on_resume && app->app_screen) {
    app->on_resume(app->app_screen);
  }

  if (set_active_screen(&app->app_screen_entry)) {
    emit_gfx_err();
    return GFX_ERR;
  }
  app->status = RUNNING;
  current_app_idx = index;

  return GFX_OK;
}

GFX_CALL app_manager_loop() {
  for (int i = 0; i < MAX_APPS; ++i) {
    app_t *app = apps[i];
    if (!app)
      continue;

    if (app->status == RUNNING || app->status == BACKGROUND) {
      if (app->on_update && app->app_screen)
        app->on_update(app->app_screen);
    }

    if (app->status == KILLED) {
      if (app->on_kill && app->app_screen)
        app->on_kill(app->app_screen);
      // TODO: Default kill clean up
      apps[i] = NULL;
    }
  }

  return GFX_OK;
}
