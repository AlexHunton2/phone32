#ifndef _APP_H_
#define _APP_H_

#include "conf.h"

typedef enum app_status {
  READY,      // The app has just been created and is ready to be started
  RUNNING,    // The app is the current one running.
  SUSPENDED,  // The app is suspended and won't have it's update function
              // called, but it's screen is still initalized
  BACKGROUND, // The app is running in the background and it's update function
              // is called.
  KILLED // The app has been marked for death by Ceasar, and the legion obeys!
         // Ready yourself for battle. (Everything about it is being deleted and
         // requires restart).
} app_status_t;

typedef struct app {
  const char *name;
  const char *icon;
  app_status_t status;
  bool has_init;
  bool should_suspend_on_close;
  screen_entry_t app_screen_entry;
  lv_obj_t *app_screen;
  void (*on_init)(lv_obj_t *);   // Called when app becomes active
  void (*on_resume)(lv_obj_t *); // Call when app comes back to the main
  void (*on_signal)(
      void *); // Called when a signal is received (e.g., phone call)
  void (*on_update)(lv_obj_t *); // Called periodically
  void (*on_background)(
      lv_obj_t *);                // Called when put into the background state
  void (*on_suspend)(lv_obj_t *); // Called when app is being suspended
  void (*on_kill)(lv_obj_t *);    // Called when app is being killed
} app_t;

extern app_t *apps[MAX_APPS];
extern int16_t current_app_idx;

#endif
