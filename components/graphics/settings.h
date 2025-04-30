#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include "lvgl.h"
#include <stdio.h>
#include <string.h>

typedef enum {
  SETTING_TYPE_SLIDER,
  SETTING_TYPE_DROPDOWN,
  SETTING_TYPE_TEXT
} setting_type_t;

typedef struct {
  int min;
  int max;
} slider_meta_t;

typedef struct {
  const char *options; // newline-separated string
} dropdown_meta_t;

typedef struct {
  int max_length;
} text_meta_t;

typedef union {
  slider_meta_t slider;
  dropdown_meta_t dropdown;
  text_meta_t text;
} setting_metadata_t;

typedef void (*setting_callback_t)(const char *key);

typedef struct {
  const char *key;
  const char *label;
  setting_type_t type;
  union {
    int slider_val;
    const char *dropdown_val;
    const char *text_val;
  } value;
  setting_metadata_t meta;
  lv_obj_t *widget;
  setting_callback_t on_change;
} setting_t;

#define MAX_SETTINGS 32
static setting_t settings[MAX_SETTINGS];
static int setting_count = 0;

void settings_add_slider(const char *key, const char *label, int default_val,
                         int min, int max, setting_callback_t cb);
void settings_add_dropdown(const char *key, const char *label,
                           const char *default_val, const char *options,
                           setting_callback_t cb);
void settings_add_text(const char *key, const char *label,
                       const char *default_val, int max_length,
                       setting_callback_t cb);
void settings_init_ui(lv_obj_t *parent);
const char *settings_get(const char *key);
lv_obj_t *settings_get_widget(const char *key);

#endif
