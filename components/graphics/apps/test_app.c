#include "apps.h"
#include "conf.h"

static lv_obj_t *label;

void init_test_app(lv_obj_t *app_scr) {
  label = lv_label_create(app_scr);
  lv_label_set_text(label, "Test App!");
  lv_obj_center(label);
}

void update_test_app(lv_obj_t *app_scr) {}

app_t test_app = {.name = "Test App",
                  .icon = LV_SYMBOL_GPS,
                  .status = READY,
                  .has_init = false,
                  .should_suspend_on_close = false,
                  .on_init = init_test_app,
                  .on_update = update_test_app};
