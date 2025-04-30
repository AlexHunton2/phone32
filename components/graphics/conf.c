#include "conf.h"

#define MAX_APPS 99

// error handling:
char gfx_err[128] = {0};

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
