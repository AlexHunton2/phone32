#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include "esp_log.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "drivers.h"

#define GFX_OK 0;
#define GFX_ERR 1;
#define GFX_CALL int

extern char gfx_err[128];

static const char *GRAPHICS_TAG = "graphics";

#define MAX_APPS 99;

typedef enum { SCREEN_LOCK, SCREEN_HOME, MAIN_SCREEN_COUNT } main_screen_id_t;

typedef struct screen_entry {
  lv_obj_t *root;
  int16_t idx;
  GFX_CALL (*init_screen)(void);
} screen_entry_t;

typedef struct app_entry {
  const char *name;
  screen_entry_t *app_screen;
} app_registry_t;

extern screen_entry_t main_screens[MAIN_SCREEN_COUNT];
extern screen_entry_t *current_screen;

void emit_gfx_err(void);

static lv_style_t style_impact_60;

/*
 * Publicly exposed functions of the phone32 graphics API
 *
 * All will return either GFX_OK or GFX_ERR. Will set gfx_err on error.
 */
GFX_CALL set_active_screen_by_main_id(main_screen_id_t id);
GFX_CALL set_active_screen(screen_entry_t *screen_entry);
GFX_CALL graphics_init(void);

#endif
