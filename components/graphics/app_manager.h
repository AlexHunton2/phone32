#ifndef _APP_MANAGER_H_
#define _APP_MANAGER_H_

#include "app.h"
#include "conf.h"
#include "screens.h"

#include <stdint.h>

GFX_CALL app_manager_init();
GFX_CALL app_manager_register_app(app_t *app, int *idx);
GFX_CALL
app_manager_open(app_t *app); // Opens an app, updates previous current app
GFX_CALL app_manager_send_signal(void *signal);
GFX_CALL app_manager_loop(); // Called inside main loop
GFX_CALL app_manager_suspend(app_t *app);
GFX_CALL app_manager_background(app_t *app);
GFX_CALL app_manager_kill(app_t *app);

#endif
