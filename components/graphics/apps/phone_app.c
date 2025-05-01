#include "apps.h"
#include "conf.h"
#include "core/lv_obj_style.h"
#include "core/lv_obj_style_gen.h"
#include "esp_log.h"
#include "font/lv_symbol_def.h"
#include "misc/lv_palette.h"
#include "widgets/textarea/lv_textarea.h"
#include "voip.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static lv_obj_t *ta_phone;
static char raw_number[11] = ""; // Only digits, max 10 + null terminator

static lv_obj_t *scr_return;

static void hangup_btn_event_cb(lv_event_t *e) {
  if (scr_return)
    lv_scr_load_anim(scr_return, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, true);
}

void ui_show_calling_screen(const char *formatted_number,
                            lv_obj_t *return_screen) {
  scr_return = return_screen; // store the screen to return to

  lv_obj_t *scr_calling = lv_obj_create(NULL);
  lv_obj_set_size(scr_calling, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(scr_calling,
                            lv_palette_darken(LV_PALETTE_LIGHT_GREEN, 2), 0);
  lv_obj_set_flex_flow(scr_calling, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(scr_calling, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  // "Calling..." label
  lv_obj_t *label_status = lv_label_create(scr_calling);
  lv_label_set_text(label_status, "Calling...");
  lv_obj_set_style_text_color(label_status, lv_color_white(), 0);
  lv_obj_set_style_text_font(label_status, &lv_font_montserrat_22, 0);

  // Phone number label
  lv_obj_t *label_number = lv_label_create(scr_calling);
  lv_label_set_text(label_number, formatted_number);
  lv_obj_set_style_text_color(label_number, lv_color_white(), 0);
  lv_obj_set_style_text_font(label_number, &lv_font_montserrat_28, 0);

  // Hangup button
  lv_obj_t *btn_hangup = lv_btn_create(scr_calling);
  lv_obj_set_size(btn_hangup, 120, 50);
  lv_obj_set_style_radius(btn_hangup, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btn_hangup, lv_palette_main(LV_PALETTE_RED), 0);
  lv_obj_add_event_cb(btn_hangup, hangup_btn_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *label_hangup = lv_label_create(btn_hangup);
  lv_label_set_text(label_hangup, "Hang Up");
  lv_obj_center(label_hangup);

  lv_scr_load_anim(scr_calling, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
}

// Formats raw_number into "(123) 456-7890"
static void update_formatted_number_display(void) {
  char formatted[20] = "";

  int len = strlen(raw_number);
  if (len == 0) {
    lv_textarea_set_text(ta_phone, "");
    return;
  }

  if (len < 4) {
    snprintf(formatted, sizeof(formatted), "(%.*s)", len, raw_number);
  } else if (len < 7) {
    snprintf(formatted, sizeof(formatted), "(%.3s) %.*s", raw_number, len - 3,
             raw_number + 3);
  } else {
    snprintf(formatted, sizeof(formatted), "(%.3s) %.3s-%.*s", raw_number,
             raw_number + 3, len - 6, raw_number + 6);
  }

  lv_textarea_set_text(ta_phone, formatted);
}

static void btn_number_event_cb(lv_event_t *e) {
  lv_obj_t *btn = lv_event_get_target(e);
  const char *text =
      lv_btnmatrix_get_btn_text(btn, lv_btnmatrix_get_selected_btn(btn));

  if (!text)
    return;

  if (strcmp(text, LV_SYMBOL_BACKSPACE) == 0) {
    size_t len = strlen(raw_number);
    if (len > 0) {
      raw_number[len - 1] = '\0';
    }
  } else if (isdigit((unsigned char)text[0]) && strlen(raw_number) < 10) {
    size_t len = strlen(raw_number);
    raw_number[len] = text[0];
    raw_number[len + 1] = '\0';
  }

  update_formatted_number_display();
}

static void call_btn_event_cb(lv_event_t *e) {
  printf("Dialing raw number: %s\n", raw_number);
  ui_show_calling_screen(lv_textarea_get_text(ta_phone), lv_scr_act());
  //xTaskCreate(make_call, "MakeCall", 8000, NULL, 1, NULL);
  make_call(raw_number);
}

#define BACK LV_SYMBOL_BACKSPACE // here for linter problems

void init_app(lv_obj_t *app_scr) {
  // Main container fills screen
  lv_obj_t *main_cont = lv_obj_create(app_scr);
  lv_obj_remove_style_all(main_cont);
  lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(main_cont, 10, 0);
  lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);

  // Textarea for phone number
  ta_phone = lv_textarea_create(main_cont);
  lv_obj_remove_style_all(ta_phone);
  lv_textarea_set_placeholder_text(ta_phone, "Enter number...");
  lv_textarea_set_one_line(ta_phone, true);
  lv_obj_set_width(ta_phone, LV_PCT(100));
  lv_obj_set_style_text_font(ta_phone, &lv_font_montserrat_28, 0);
  lv_obj_add_flag(ta_phone, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_set_style_text_align(ta_phone, LV_TEXT_ALIGN_CENTER, 0);

  // Keypad map including backspace
  static const char *btnm_map[] = {"1", "2",  "3", "\n", "4",  "5",
                                   "6", "\n", "7", "8",  "9",  "\n",
                                   "*", "0",  "#", "\n", BACK, ""};

  // Keypad
  lv_obj_t *btnm = lv_btnmatrix_create(main_cont);
  lv_obj_set_style_border_width(btnm, 0, 0);
  lv_btnmatrix_set_map(btnm, btnm_map);
  lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_CLICK_TRIG);
  lv_obj_set_size(btnm, LV_PCT(100), LV_PCT(75));
  lv_obj_add_event_cb(btnm, btn_number_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // Make buttons circular by customizing style
  static lv_style_t style_btn;
  lv_style_init(&style_btn);
  lv_style_set_radius(&style_btn, LV_RADIUS_CIRCLE);
  lv_style_set_pad_all(&style_btn, 10);
  lv_style_set_text_font(&style_btn, &lv_font_montserrat_20);

  lv_obj_add_style(btnm, &style_btn, LV_PART_ITEMS);

  // Call button
  lv_obj_t *btn_call = lv_btn_create(main_cont);
  lv_obj_set_size(btn_call, LV_PCT(100), LV_PCT(15));
  lv_obj_set_style_radius(btn_call, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btn_call, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_obj_add_style(btn_call, &style_btn, 0);

  lv_obj_t *label = lv_label_create(btn_call);
  lv_label_set_text(label, "Call");
  lv_obj_center(label);

  lv_obj_add_event_cb(btn_call, call_btn_event_cb, LV_EVENT_CLICKED, NULL);
}

void resume_app(lv_obj_t *app_src) {
  (void)app_src;
  memset(raw_number, 0, sizeof(raw_number));
  lv_textarea_set_text(ta_phone, "");
}

app_t phone_app = {.name = "Phone",
                   .icon = LV_SYMBOL_CALL,
                   .status = READY,
                   .has_init = false,
                   .should_suspend_on_close = false,
                   .on_init = init_app,
                   .on_resume = resume_app};
