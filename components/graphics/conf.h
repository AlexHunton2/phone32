#ifndef _CONF_H_
#define _CONF_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "esp_log.h"
#include "lvgl.h"

/*
 * Publicly exposed functions of the phone32 graphics API use GFX_CALL
 *
 * All will return either GFX_OK or GFX_ERR. Will set gfx_err on error.
 * These must be used in conjunction with the lvport_lock/lvport_unlock for
 * thread safety.
 */
#define GFX_OK 0;
#define GFX_ERR 1;
#define GFX_CALL uint8_t

#define MAX_APPS 99
#define APP_LOOP_DELAY_MS 100 // e.g., 10 FPS update rate

#define SCREEN_H 800
#define SCREEN_W 480
#define NAVBAR_H 80

typedef struct screen_entry {
  lv_obj_t *root;
  int16_t idx; // -1 for apps, 0 to MAIN_SCREEN_COUNT for main screens
  GFX_CALL (*init_screen)(void); // Only used for main screens
} screen_entry_t;

extern char gfx_err[128];

void emit_gfx_err(void);
void append_error(const char *fmt, ...);

static const char *GRAPHICS_TAG = "graphics";

typedef struct {
  const char *label;
  const char *tz_value;
} TimeZoneOption;

static const TimeZoneOption time_zones[] = {
    {"UTC", "UTC0"},
    {"US Eastern", "EST5EDT,M3.2.0,M11.1.0"},
    {"US Central", "CST6CDT,M3.2.0,M11.1.0"},
    {"US Mountain", "MST7MDT,M3.2.0,M11.1.0"},
    {"US Pacific", "PST8PDT,M3.2.0,M11.1.0"},
    {"London", "GMT0BST,M3.5.0/1,M10.5.0/2"},
    {"Berlin", "CET-1CEST,M3.5.0/2,M10.5.0/3"},
    {"Tokyo", "JST-9"},
    {"Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"}};

#endif
