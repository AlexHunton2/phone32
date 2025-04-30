#include "apps.h"

static lv_obj_t *label;
static int counter = 0;

void init_background_app(lv_obj_t *app_scr) {
  label = lv_label_create(app_scr);
  lv_label_set_text(label, "Counter: 0");
  lv_obj_center(label);
}

void update_background_app(lv_obj_t *app_scr) {
  static int i = 0;
  i++;
  if (i == 10) {
    i = 0;
    counter++;
    static char buf[32];
    snprintf(buf, sizeof(buf), "Counter: %d", counter);
    lv_label_set_text(label, buf);
  }
}

app_t background_app = {.name = "Background App",
                        .icon = LV_SYMBOL_PLAY,
                        .status = READY,
                        .has_init = false,
                        .should_suspend_on_close = false,
                        .on_init = init_background_app,
                        .on_update = update_background_app};
