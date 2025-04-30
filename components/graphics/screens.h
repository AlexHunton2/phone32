#ifndef _SCREENS_H_
#define _SCREENS_H_

#include "app.h"
#include "conf.h"

#include "esp_log.h"
#include "lvgl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

typedef enum {
  SCREEN_LOCK,
  SCREEN_HOME,
  SCREEN_SETTINGS,
  MAIN_SCREEN_COUNT
} main_screen_id_t;

extern screen_entry_t main_screens[MAIN_SCREEN_COUNT];
extern screen_entry_t *current_screen;

static lv_style_t style_impact_60;

GFX_CALL set_active_screen_by_main_id(main_screen_id_t id);
GFX_CALL set_active_screen(screen_entry_t *screen_entry);
GFX_CALL init_screens(void);

#endif
