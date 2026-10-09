/*
 * Pebble Roon Remote - Entry & App Lifecycle
 * Copyright (c) 2026 J_B
 */

#include <pebble.h>
#include "app_state.h"

AppState g_state;

static AppTimer *s_app_idle_timer = NULL;
static AppTimer *s_disc_idle_timer = NULL;

static void exit_app_cb(void *data) {
  if (g_state.enable_watchface && g_state.timeout_to_app_wf) {
    if (g_state.mode == MODE_ZONE) {
      g_state.mode = MODE_TRACK;
    }
    input_exit_gesture_mode();
  } else {
    window_stack_pop_all(true);
  }
}

void app_cancel_idle_timers(void) {
  if (s_app_idle_timer) { app_timer_cancel(s_app_idle_timer); s_app_idle_timer = NULL; }
  if (s_disc_idle_timer) { app_timer_cancel(s_disc_idle_timer); s_disc_idle_timer = NULL; }
}

void app_mark_user_interaction(void) {
  if (s_app_idle_timer) { app_timer_cancel(s_app_idle_timer); s_app_idle_timer = NULL; }
  if (g_state.timeout_app_min > 0 && g_state.mode != MODE_ERROR && !g_state.is_configuring) {
    s_app_idle_timer = app_timer_register(g_state.timeout_app_min * 1000, exit_app_cb, NULL);
  }
  if (g_state.gesture_mode_active) {
    input_extend_gesture_mode();
  }
}

void app_start_disconnect_timer(void) {
  app_cancel_idle_timers();
  if (g_state.timeout_disc_min > 0 && !g_state.is_configuring) {
    s_disc_idle_timer = app_timer_register(g_state.timeout_disc_min * 1000, exit_app_cb, NULL);
  }
}

static void bluetooth_callback(bool connected) {
  if (connected) {
    if (g_state.mode == MODE_ERROR) {
      g_state.mode = MODE_TRACK;
      app_cancel_idle_timers();
      app_mark_user_interaction();
      ui_update();
      comm_send_command("status");
    }
  } else {
    if (g_state.mode != MODE_ERROR) {
      g_state.mode = MODE_ERROR;
      app_start_disconnect_timer();
      ui_update();
    }
  }
}

static void focus_handler(bool in_focus) {
  g_state.app_in_focus = in_focus;
  if (in_focus) {
    if (g_state.scroll_mode == 1) ui_start_marquee();
  } else {
    ui_stop_marquee();
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (units_changed & MINUTE_UNIT) {
    ui_tick_handler(tick_time);
  }
}

extern void accel_tap_handler(AccelAxisType axis, int32_t direction);

void app_update_subscriptions(void) {
  if (g_state.enable_watchface) {
    tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT, tick_handler);
  } else {
    tick_timer_service_unsubscribe();
  }

  if (g_state.enable_watchface || g_state.enable_accel_playpause) {
    accel_tap_service_subscribe(accel_tap_handler);
  } else {
    accel_tap_service_unsubscribe();
  }
}

static void init(void) {
  g_state.mode = MODE_TRACK;
  g_state.font_size = 1;
  g_state.scroll_mode = 0;
  g_state.enable_touch = true;
  g_state.enable_watchface = false;
  g_state.respect_quiet_time = true;
  g_state.enable_accel_playpause = true;
  g_state.timeout_to_app_wf = true;
  g_state.suppress_gesture_quiet = false;
  g_state.enable_flick_vibes = true;
  g_state.app_in_focus = true;
  g_state.volume = -1;

  if (persist_exists(PERSIST_KEY_FONT)) g_state.font_size = persist_read_int(PERSIST_KEY_FONT);
  if (persist_exists(PERSIST_KEY_SCROLL)) g_state.scroll_mode = persist_read_int(PERSIST_KEY_SCROLL);
  if (persist_exists(PERSIST_KEY_TIMEOUT_APP)) g_state.timeout_app_min = persist_read_int(PERSIST_KEY_TIMEOUT_APP);
  if (persist_exists(PERSIST_KEY_TIMEOUT_DISC)) g_state.timeout_disc_min = persist_read_int(PERSIST_KEY_TIMEOUT_DISC);
  if (persist_exists(PERSIST_KEY_TOUCH)) g_state.enable_touch = persist_read_bool(PERSIST_KEY_TOUCH);
  if (persist_exists(PERSIST_KEY_WATCHFACE)) g_state.enable_watchface = persist_read_bool(PERSIST_KEY_WATCHFACE);
  if (persist_exists(PERSIST_KEY_QUIET_TIME)) g_state.respect_quiet_time = persist_read_bool(PERSIST_KEY_QUIET_TIME);
  if (persist_exists(PERSIST_KEY_ACCEL)) g_state.enable_accel_playpause = persist_read_bool(PERSIST_KEY_ACCEL);
  if (persist_exists(PERSIST_KEY_THEME)) g_state.theme = persist_read_int(PERSIST_KEY_THEME);
  if (persist_exists(PERSIST_KEY_TIMEOUT_DEST)) g_state.timeout_to_app_wf = persist_read_bool(PERSIST_KEY_TIMEOUT_DEST);
  if (persist_exists(PERSIST_KEY_SUPPRESS_GESTURE_QT)) g_state.suppress_gesture_quiet = persist_read_bool(PERSIST_KEY_SUPPRESS_GESTURE_QT);
  if (persist_exists(PERSIST_KEY_FLICK_VIBES)) g_state.enable_flick_vibes = persist_read_bool(PERSIST_KEY_FLICK_VIBES);

  g_state.window = window_create();
  window_set_window_handlers(g_state.window, (WindowHandlers) { .load = ui_window_load, .unload = ui_window_unload });

  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .will_focus = focus_handler,
    .did_focus = focus_handler
  });

  connection_service_subscribe((ConnectionHandlers) { .pebble_app_connection_handler = bluetooth_callback });

  input_init(g_state.window);
  comm_init();

  app_update_subscriptions();

  window_stack_push(g_state.window, true);

  if (!connection_service_peek_pebble_app_connection()) {
    g_state.mode = MODE_ERROR;
    app_start_disconnect_timer();
    ui_update();
  } else {
    comm_send_command("status");
  }
}

static void deinit(void) {
  app_focus_service_unsubscribe();
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  connection_service_unsubscribe();

  input_deinit();
  comm_deinit();

  window_destroy(g_state.window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
