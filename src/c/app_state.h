/*
 * Pebble Roon Remote - Shared State
 * Copyright (c) 2026 J_B
 */

#pragma once
#include <pebble.h>

#define ENABLE_VOLUME 1
#define GESTURE_MODE_TIMEOUT_MS 5000

// AppMessage Keys
#define KEY_COMMAND 0
#define KEY_ZONE_NAME 1
#define KEY_TRACK 2
#define KEY_ARTIST 3
#define KEY_IS_PLAYING 4
#define KEY_VOLUME_VAL 5
#define KEY_IS_FIXED 6
#define KEY_ERROR 7
#define KEY_FONT_SIZE 8
#define KEY_SCROLL_TEXT 9
#define KEY_TIMEOUT_APP 10
#define KEY_TIMEOUT_DISC 11
#define KEY_ENABLE_TOUCH 12
#define KEY_IS_CONFIGURING 13
#define KEY_THEME 14
#define KEY_ENABLE_WATCHFACE 15
#define KEY_RESPECT_QUIET_TIME 16
#define KEY_ENABLE_ACCEL_PLAYPAUSE 17
#define KEY_TIMEOUT_TO_APP_WF 18
#define KEY_SUPPRESS_GESTURE_QUIET 19
#define KEY_ENABLE_FLICK_VIBES 20

// Persistence Keys
#define PERSIST_KEY_FONT 0
#define PERSIST_KEY_SCROLL 1
#define PERSIST_KEY_TIMEOUT_APP 2
#define PERSIST_KEY_TIMEOUT_DISC 3
#define PERSIST_KEY_TOUCH 4
#define PERSIST_KEY_THEME 5
#define PERSIST_KEY_WATCHFACE 6
#define PERSIST_KEY_QUIET_TIME 7
#define PERSIST_KEY_ACCEL 8
#define PERSIST_KEY_TIMEOUT_DEST 9
#define PERSIST_KEY_SUPPRESS_GESTURE_QT 10
#define PERSIST_KEY_FLICK_VIBES 11

typedef enum {
  MODE_TRACK,
  MODE_ZONE,
  MODE_ERROR
} AppMode;

typedef struct {
  AppMode mode;
  int font_size;
  int scroll_mode;
  int timeout_app_min;
  int timeout_disc_min;
  bool enable_touch;
  int theme;
  bool enable_watchface;
  bool respect_quiet_time;
  bool enable_accel_playpause;
  bool timeout_to_app_wf;
  bool suppress_gesture_quiet;
  bool enable_flick_vibes;

  bool gesture_mode_active;
  bool btns_locked;
  bool is_playing;
  bool is_fixed;
  bool app_in_focus;
  bool is_configuring;
  int volume;

  char track_buf[128];
  char artist_buf[128];
  char zone_buf[64];
  char vol_buf[32];

  bool ignore_play_updates;
  bool ignore_vol_updates;
  bool is_flashing_vol;

  Window *window;
  bool window_loaded;
} AppState;

extern AppState g_state;

// --- App Lifecycle & Timers (main.c) ---
void app_mark_user_interaction(void);
void app_start_disconnect_timer(void);
void app_cancel_idle_timers(void);
void app_update_subscriptions(void);

// --- User Interface Engine (ui.c) ---
void ui_window_load(Window *window);
void ui_window_unload(Window *window);
void ui_update(void);
void ui_apply_theme(void);
void ui_apply_layout(void);
void ui_apply_fonts(void);
void ui_start_marquee(void);
void ui_stop_marquee(void);
bool ui_is_marquee_running(void);
void ui_flash_volume(int ms);
void ui_cancel_vol_flash(void);
void ui_show_temporary_message(const char *track, const char *artist);
void ui_tick_handler(struct tm *tick_time);

// --- Bridge Communication (comm.c) ---
void comm_init(void);
void comm_deinit(void);
void comm_send_command(const char *cmd);

// --- Input & Gesture Engine (input.c) ---
void input_init(Window *window);
void input_deinit(void);
void input_enter_gesture_mode(void);
void input_extend_gesture_mode(void);
void input_exit_gesture_mode(void);
