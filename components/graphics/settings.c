#include "settings.h"
#include "core/lv_obj_style.h"
#include "font/lv_font.h"
#include "layouts/flex/lv_flex.h"
#include "misc/lv_style_gen.h"
#include "misc/lv_types.h"

static setting_t settings[MAX_SETTINGS] = {0};

void settings_add_slider(const char *key, const char *label, int default_val,
                         int min, int max, setting_callback_t cb) {
  if (setting_count >= MAX_SETTINGS)
    return;
  setting_t *s = &settings[setting_count++];
  s->key = key;
  s->label = label;
  s->type = SETTING_TYPE_SLIDER;
  s->value.slider_val = default_val;
  s->meta.slider.min = min;
  s->meta.slider.max = max;
  s->on_change = cb;
}

void settings_add_dropdown(const char *key, const char *label,
                           const char *default_val, const char *options,
                           setting_callback_t cb) {
  if (setting_count >= MAX_SETTINGS)
    return;
  setting_t *s = &settings[setting_count++];
  s->key = key;
  s->label = label;
  s->type = SETTING_TYPE_DROPDOWN;
  s->value.dropdown_val = default_val;
  s->meta.dropdown.options = options;
  s->on_change = cb;
}

void settings_add_text(const char *key, const char *label,
                       const char *default_val, int max_length,
                       setting_callback_t cb) {
  if (setting_count >= MAX_SETTINGS)
    return;
  setting_t *s = &settings[setting_count++];
  s->key = key;
  s->label = label;
  s->type = SETTING_TYPE_TEXT;
  s->value.text_val = default_val;
  s->meta.text.max_length = max_length;
  s->on_change = cb;
}

static void slider_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target(e);
  for (int j = 0; j < setting_count; j++) {
    if (settings[j].widget == obj) {
      settings[j].value.slider_val = lv_slider_get_value(obj);
      if (settings[j].on_change) {
        settings[j].on_change(settings[j].key);
      }
      break;
    }
  }
}

static void dropdown_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target(e);
  for (int j = 0; j < setting_count; j++) {
    if (settings[j].widget == obj) {
      settings[j].value.dropdown_val = lv_dropdown_get_text(obj);
      if (settings[j].on_change) {
        settings[j].on_change(settings[j].key);
      }
      break;
    }
  }
}

static void text_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target(e);
  for (int j = 0; j < setting_count; j++) {
    if (settings[j].widget == obj) {
      settings[j].value.text_val = lv_textarea_get_text(obj);
      if (settings[j].on_change) {
        settings[j].on_change(settings[j].key);
      }
      break;
    }
  }
}

void settings_init_ui(lv_obj_t *parent) {
  lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(parent, 8, 0); // space between rows

  static lv_style_t setting_label_style;
  lv_style_init(&setting_label_style);
  lv_style_set_text_font(&setting_label_style, &lv_font_montserrat_22);

  for (int i = 0; i < setting_count; i++) {
    setting_t *s = &settings[i];

    // Row container with horizontal flex layout
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(row, 12, 0); // spacing between label and widget
    lv_obj_set_style_pad_all(row, 4, 0);  // padding inside row
    lv_obj_set_style_align(row, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Label
    lv_obj_t *label = lv_label_create(row);
    lv_obj_add_style(label, &setting_label_style, 0);
    lv_label_set_text(label, s->label);

    switch (s->type) {
    case SETTING_TYPE_SLIDER: {
      lv_obj_t *slider = lv_slider_create(row);
      lv_slider_set_range(slider, s->meta.slider.min, s->meta.slider.max);
      lv_slider_set_value(slider, s->value.slider_val, LV_ANIM_OFF);
      s->widget = slider;

      lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
      break;
    }

    case SETTING_TYPE_DROPDOWN: {
      lv_obj_t *dd = lv_dropdown_create(row);
      lv_dropdown_set_options(dd, s->meta.dropdown.options);
      s->widget = dd;

      // Try to select the default option by index
      uint16_t selected = 0;
      char options_copy[256];
      strncpy(options_copy, s->meta.dropdown.options, sizeof(options_copy) - 1);
      options_copy[sizeof(options_copy) - 1] = '\0';

      char *token = strtok(options_copy, "\n");
      while (token) {
        if (strcmp(token, s->value.dropdown_val) == 0) {
          break;
        }
        selected++;
        token = strtok(NULL, "\n");
      }
      lv_dropdown_set_selected(dd, selected, LV_ANIM_ON);

      lv_obj_add_event_cb(dd, dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);
      break;
    }

    case SETTING_TYPE_TEXT: {
      lv_obj_t *ta = lv_textarea_create(row);
      lv_textarea_set_text(ta, s->value.text_val);
      lv_textarea_set_max_length(ta, s->meta.text.max_length);
      s->widget = ta;

      lv_obj_add_event_cb(ta, text_cb, LV_EVENT_DEFOCUSED, NULL);
      break;
    }
    }
  }
}

const char *settings_get(const char *key) {
  for (int i = 0; i < setting_count; i++) {
    if (strcmp(settings[i].key, key) == 0) {
      static char buffer[64];
      switch (settings[i].type) {
      case SETTING_TYPE_SLIDER:
        snprintf(buffer, sizeof(buffer), "%d", settings[i].value.slider_val);
        return buffer;
      case SETTING_TYPE_DROPDOWN:
        return settings[i].value.dropdown_val;
      case SETTING_TYPE_TEXT:
        return settings[i].value.text_val;
      }
    }
  }
  return NULL;
}

lv_obj_t *settings_get_widget(const char *key) {
  for (int i = 0; i < setting_count; i++) {
    if (strcmp(settings[i].key, key) == 0) {
      return settings[i].widget;
    }
  }
  return NULL;
}
