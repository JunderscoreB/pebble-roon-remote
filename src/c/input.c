/*
 * Pebble Roon Remote - Event Handling & Gestures
 * Copyright (c) 2026 J_B
 */

#include <pebble.h>
#include <stdlib.h>
#include "app_state.h"

static AppTimer *s_playpause_delay_timer = NULL;
static AppTimer *s_zone_revert_timer = NULL;
static AppTimer *s_btn_lock_timer = NULL;
static AppTimer *s_gesture_mode_timer = NULL;
static AppTimer *s_touch_hold_timer = NULL;
static AppTimer *s_vol_ready_timer = NULL;
static AppTimer *s_play_ignore_timer = NULL;

#if ENABLE_VOLUME
static AppTimer *s_vol_ignore_timer = NULL;
static int16_t s_last_vol_y = -1;
static bool s_volume_mode_ready = false;
static bool s_volume_mode_active = false;
static int16_t s_touch_start_x = -1;
static int16_t s_touch_start_y = -1;
static bool s_touch_held = false;
#endif

static void extend_gesture_mode(void);

static void btn_unlock_cb(void *data) {
  g_state.btns_locked = false;
  s_btn_lock_timer = NULL;
}

static void lock_buttons_temporarily(int delay_ms) {
  g_state.btns_locked = true;
  if (s_btn_lock_timer) app_timer_cancel(s_btn_lock_timer);
  s_btn_lock_timer = app_timer_register(delay_ms, btn_unlock_cb, NULL);
}

static void zone_revert_callback(void *data) {
  s_zone_revert_timer = NULL;
  if (g_state.mode == MODE_ZONE) { g_state.mode = MODE_TRACK; ui_update(); }
}

void input_reset_zone_timer() {
  if (s_zone_revert_timer) app_timer_cancel(s_zone_revert_timer);
  s_zone_revert_timer = app_timer_register(8000, zone_revert_callback, NULL);
}

void input_cancel_zone_timer() {
  if (s_zone_revert_timer) { app_timer_cancel(s_zone_revert_timer); s_zone_revert_timer = NULL; }
}

#if ENABLE_VOLUME
static void vol_ignore_cb(void *data) {
  g_state.ignore_vol_updates = false;
  s_vol_ignore_timer = NULL;
}

void input_lock_volume_updates() {
  g_state.ignore_vol_updates = true;
  if (s_vol_ignore_timer) app_timer_cancel(s_vol_ignore_timer);
  s_vol_ignore_timer = app_timer_register(1500, vol_ignore_cb, NULL);
}
#endif

static void play_ignore_cb(void *data) {
  g_state.ignore_play_updates = false;
  s_play_ignore_timer = NULL;
}

static void send_playpause_cb(void *data) {
  s_playpause_delay_timer = NULL;
  comm_send_command("playpause");
}

void input_trigger_optimistic_playpause() {
  g_state.is_playing = !g_state.is_playing;
  g_state.ignore_play_updates = true;
  if (s_play_ignore_timer) app_timer_cancel(s_play_ignore_timer);
  s_play_ignore_timer = app_timer_register(2000, play_ignore_cb, NULL);

  ui_update();
  vibes_short_pulse();
  if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
  s_playpause_delay_timer = app_timer_register(100, send_playpause_cb, NULL);
}

static void end_gesture_mode_cb(void *data) {
  s_gesture_mode_timer = NULL;

  if (ui_is_marquee_running() || g_state.mode == MODE_ZONE || g_state.is_flashing_vol) {
    input_extend_gesture_mode();
    return;
  }

  g_state.gesture_mode_active = false;
  ui_stop_marquee();
  ui_apply_layout();
  ui_update();

  comm_send_command("idle_true");

  if (g_state.scroll_mode == 1) ui_start_marquee();
}

void input_extend_gesture_mode(void) {
  if (s_gesture_mode_timer) {
    app_timer_reschedule(s_gesture_mode_timer, GESTURE_MODE_TIMEOUT_MS);
  } else {
    s_gesture_mode_timer = app_timer_register(GESTURE_MODE_TIMEOUT_MS, end_gesture_mode_cb, NULL);
  }
}

void input_enter_gesture_mode(void) {
  if (!g_state.gesture_mode_active) {
    g_state.gesture_mode_active = true;

    if (g_state.enable_flick_vibes) {
      vibes_short_pulse();
    }

    ui_stop_marquee();
    ui_apply_layout();
    ui_update();

    comm_send_command("idle_false");

    if (g_state.scroll_mode == 1) ui_start_marquee();
  }
  input_extend_gesture_mode();
}

void input_exit_gesture_mode(void) {
  if (s_gesture_mode_timer) {
    app_timer_cancel(s_gesture_mode_timer);
    s_gesture_mode_timer = NULL;
  }
  g_state.gesture_mode_active = false;
  ui_stop_marquee();
  ui_apply_layout();
  ui_update();

  comm_send_command("idle_true");

  if (g_state.scroll_mode == 1) ui_start_marquee();
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_mark_user_interaction();

  #if ENABLE_VOLUME
  // Dismiss the volume overlay if it's currently active
  if (g_state.is_flashing_vol) {
    ui_cancel_vol_flash();
    ui_update();
    return;
  }
  #endif

  if (g_state.mode == MODE_ZONE) {
    input_cancel_zone_timer();
    g_state.mode = MODE_TRACK;
    ui_update();
  } else {
    window_stack_pop(true);
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_mark_user_interaction();

  if (g_state.enable_watchface && !g_state.gesture_mode_active) {
    input_enter_gesture_mode();
    return;
  }

  if (g_state.mode == MODE_ERROR) return;

  if (g_state.mode == MODE_TRACK) {
    #if ENABLE_VOLUME
    if (!g_state.is_fixed) {
      if (g_state.volume != -1) { g_state.volume += 2; if (g_state.volume > 100) g_state.volume = 100; }
      comm_send_command("vol_up");
      input_lock_volume_updates();
      ui_flash_volume(2000);
    } else {
      ui_flash_volume(500);
    }
    #endif
  } else if (g_state.mode == MODE_ZONE) {
    if (g_state.btns_locked) return;
    input_reset_zone_timer();
    ui_stop_marquee();
    comm_send_command("prev_zone");
    lock_buttons_temporarily(300);
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_mark_user_interaction();

  if (g_state.enable_watchface && !g_state.gesture_mode_active) {
    input_enter_gesture_mode();
    return;
  }

  if (g_state.mode == MODE_ERROR) return;

  if (g_state.mode == MODE_TRACK) {
    #if ENABLE_VOLUME
    if (!g_state.is_fixed) {
      if (g_state.volume != -1) { g_state.volume -= 2; if (g_state.volume < 0) g_state.volume = 0; }
      comm_send_command("vol_down");
      input_lock_volume_updates();
      ui_flash_volume(2000);
    } else {
      ui_flash_volume(500);
    }
    #endif
  } else if (g_state.mode == MODE_ZONE) {
    if (g_state.btns_locked) return;
    input_reset_zone_timer();
    ui_stop_marquee();
    comm_send_command("next_zone");
    lock_buttons_temporarily(300);
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_mark_user_interaction();

  if (g_state.enable_watchface && !g_state.gesture_mode_active) {
    input_enter_gesture_mode();
    return;
  }

  if (g_state.mode == MODE_ERROR) {
    ui_show_temporary_message("Retrying...", "Please wait...");
    comm_send_command("retry_connection");
    return;
  }

  if (g_state.btns_locked && g_state.mode == MODE_ZONE) return;

  ui_cancel_vol_flash();

  if (g_state.mode == MODE_TRACK) {
    g_state.mode = MODE_ZONE;
    input_reset_zone_timer();
  } else if (g_state.mode == MODE_ZONE) {
    input_cancel_zone_timer();
    comm_send_command("status");
    g_state.mode = MODE_TRACK;
  }
  ui_update();
}

static void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_mark_user_interaction();
  if (g_state.mode == MODE_ERROR || g_state.btns_locked) return;
  vibes_short_pulse();
  comm_send_command("previous");
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_mark_user_interaction();
  if (g_state.mode == MODE_ERROR || g_state.btns_locked) return;
  vibes_short_pulse();
  comm_send_command("next");
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_mark_user_interaction();
  if (g_state.mode == MODE_ERROR || g_state.btns_locked) return;
  input_trigger_optimistic_playpause();
  if (g_state.mode == MODE_ZONE) input_reset_zone_timer();
}

static void select_double_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_mark_user_interaction();
  if (g_state.mode == MODE_ERROR || g_state.btns_locked) return;

  vibes_long_pulse();
  ui_stop_marquee();
  ui_show_temporary_message("Pausing All...", "");

  g_state.track_buf[0] = '\0';
  g_state.artist_buf[0] = '\0';

  comm_send_command("pause_all");
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);

  window_long_click_subscribe(BUTTON_ID_UP, 600, up_long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 600, down_long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 800, select_long_click_handler, NULL);

  window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 2, 300, true, select_double_click_handler);
}

#if ENABLE_VOLUME
static void touch_hold_cb(void *data) {
  s_touch_hold_timer = NULL;
  s_touch_held = true;
  vibes_long_pulse();
}

static void vol_drag_ready_cb(void *data) {
  s_vol_ready_timer = NULL;
  s_volume_mode_ready = true;
}

static void touch_handler(const TouchEvent *event, void *context) {
  app_mark_user_interaction();

  if (g_state.enable_watchface && !g_state.gesture_mode_active) return;

  if (g_state.gesture_mode_active) input_extend_gesture_mode();

  if (g_state.mode == MODE_ERROR || g_state.btns_locked || !g_state.enable_touch || !g_state.app_in_focus) return;

  if (event->type == TouchEvent_Touchdown) {

    // Check if the user tapped the upper-left "back" area
    if (g_state.gesture_mode_active && event->x < 50 && event->y < 50) {

      // If the volume text is visible, dismiss it first
      #if ENABLE_VOLUME
      if (g_state.is_flashing_vol) {
        ui_cancel_vol_flash();
        ui_update();
        return;
      }
      #endif

      // Otherwise, naturally exit gesture mode
      input_exit_gesture_mode();
      return;
    }

    s_touch_start_x = event->x;
    s_touch_start_y = event->y;
    s_last_vol_y = event->y;
    s_touch_held = false;
    s_volume_mode_ready = false;
    s_volume_mode_active = false;

    ui_cancel_vol_flash();

    if (s_touch_hold_timer) app_timer_cancel(s_touch_hold_timer);
    s_touch_hold_timer = app_timer_register(600, touch_hold_cb, NULL);

    if (s_vol_ready_timer) app_timer_cancel(s_vol_ready_timer);
    s_vol_ready_timer = app_timer_register(400, vol_drag_ready_cb, NULL);
  }
  else if (event->type == TouchEvent_PositionUpdate) {
    int16_t current_x = event->x;
    int16_t current_y = event->y;
    int16_t dx = abs(current_x - s_touch_start_x);
    int16_t dy = abs(current_y - s_touch_start_y);

    if (s_volume_mode_ready && dy > 15 && dy > dx * 2) {
      if (!s_volume_mode_active) {
        s_volume_mode_active = true;
        if (s_touch_hold_timer) { app_timer_cancel(s_touch_hold_timer); s_touch_hold_timer = NULL; }
        s_touch_held = false;
        vibes_double_pulse();
      }

      int16_t step_dy = current_y - s_last_vol_y;
      if (step_dy <= -10 || step_dy >= 10) {
        if (!g_state.is_fixed) {
          if (step_dy <= -10) {
            if (g_state.volume != -1) { g_state.volume += 2; if (g_state.volume > 100) g_state.volume = 100; }
            comm_send_command("vol_up");
            input_lock_volume_updates();
          } else {
            if (g_state.volume != -1) { g_state.volume -= 2; if (g_state.volume < 0) g_state.volume = 0; }
            comm_send_command("vol_down");
            input_lock_volume_updates();
          }
          ui_flash_volume(3000);
        } else {
          ui_flash_volume(1000);
        }
        s_last_vol_y = current_y;
      }
    }
    else if (dx > 15 || dy > 15) {
      if (s_touch_hold_timer && s_touch_start_x != -1) {
        app_timer_cancel(s_touch_hold_timer);
        s_touch_hold_timer = NULL;
      }
      if (s_vol_ready_timer) {
        app_timer_cancel(s_vol_ready_timer);
        s_vol_ready_timer = NULL;
      }
    }
  }
  else if (event->type == TouchEvent_Liftoff) {
    if (s_touch_hold_timer) { app_timer_cancel(s_touch_hold_timer); s_touch_hold_timer = NULL; }
    if (s_vol_ready_timer) { app_timer_cancel(s_vol_ready_timer); s_vol_ready_timer = NULL; }

    if (s_volume_mode_active) {
      s_volume_mode_active = false;
      s_touch_start_x = -1;
      s_touch_start_y = -1;
      ui_flash_volume(3000);
      return;
    }

    if (s_touch_held) {
      s_touch_held = false;
      s_touch_start_x = -1;
      s_touch_start_y = -1;
      ui_stop_marquee();
      ui_show_temporary_message("Pausing All...", "");
      g_state.track_buf[0] = '\0';
      g_state.artist_buf[0] = '\0';
      comm_send_command("pause_all");
      return;
    }

    if (s_touch_start_x != -1 && s_touch_start_y != -1) {
      int16_t delta_x = event->x - s_touch_start_x;
      int16_t delta_y = event->y - s_touch_start_y;
      int16_t abs_dx = abs(delta_x);
      int16_t abs_dy = abs(delta_y);

      if (g_state.is_flashing_vol) {
        if (!g_state.is_fixed && abs_dy > 20 && abs_dy > abs_dx) {
          vibes_short_pulse();
          if (delta_y < 0) {
            if (g_state.volume != -1) { g_state.volume += 2; if (g_state.volume > 100) g_state.volume = 100; }
            comm_send_command("vol_up");
            input_lock_volume_updates();
          } else {
            if (g_state.volume != -1) { g_state.volume -= 2; if (g_state.volume < 0) g_state.volume = 0; }
            comm_send_command("vol_down");
            input_lock_volume_updates();
          }
        }
        ui_flash_volume(3000);
      }
      else if (abs_dx > 30 && abs_dx > abs_dy) {
        if (g_state.mode == MODE_TRACK) {
          vibes_short_pulse();
          if (delta_x > 0) comm_send_command("previous");
          else comm_send_command("next");
          lock_buttons_temporarily(300);
        }
      }
      else if (abs_dy > 30 && abs_dy > abs_dx) {
        vibes_short_pulse();
        input_reset_zone_timer();
        ui_stop_marquee();

        if (delta_y > 0) comm_send_command("prev_zone");
        else comm_send_command("next_zone");
        lock_buttons_temporarily(300);
      }
      else if (abs_dx < 10 && abs_dy < 10) {
        if (g_state.mode == MODE_TRACK) input_trigger_optimistic_playpause();
        else if (g_state.mode == MODE_ZONE) input_reset_zone_timer();
      }
    }

    s_touch_start_x = -1;
    s_touch_start_y = -1;
  }
}
#endif

void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  app_mark_user_interaction();

  if (axis == ACCEL_AXIS_Z) {
    if (g_state.enable_watchface && !g_state.gesture_mode_active) {

      bool is_quiet_time = false;
      #if !defined(PBL_BW)
      is_quiet_time = quiet_time_is_active();
      #endif

      if (g_state.suppress_gesture_quiet && is_quiet_time) {
        return;
      }

      input_enter_gesture_mode();
      return;
    }

    if (g_state.mode == MODE_ERROR || g_state.btns_locked || !g_state.app_in_focus) return;

    if (g_state.enable_accel_playpause && !touch_service_is_enabled()) {
      if (g_state.mode == MODE_TRACK) input_trigger_optimistic_playpause();
      else if (g_state.mode == MODE_ZONE) input_reset_zone_timer();
    }
  }
}

void input_init(Window *window) {
  window_set_click_config_provider(window, click_config_provider);

  if (touch_service_is_enabled()) {
    #if ENABLE_VOLUME
    touch_service_subscribe(touch_handler, NULL);
    #endif
  }
}

void input_deinit(void) {
  if (touch_service_is_enabled()) touch_service_unsubscribe();

  if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
  if (s_btn_lock_timer) app_timer_cancel(s_btn_lock_timer);
  if (s_gesture_mode_timer) app_timer_cancel(s_gesture_mode_timer);
  if (s_touch_hold_timer) app_timer_cancel(s_touch_hold_timer);
  if (s_vol_ready_timer) app_timer_cancel(s_vol_ready_timer);
  if (s_zone_revert_timer) app_timer_cancel(s_zone_revert_timer);
  if (s_play_ignore_timer) app_timer_cancel(s_play_ignore_timer);
  #if ENABLE_VOLUME
  if (s_vol_ignore_timer) app_timer_cancel(s_vol_ignore_timer);
  #endif
}
