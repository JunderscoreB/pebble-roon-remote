/*
 * Pebble Roon Remote
 * Copyright (c) 2026 J_B
 *
 * Released under the MIT License.
 *
 * AI Disclosure: Portions of this file were generated and optimized with the assistance of generative AI.
 * Co-Authored-By: Google Gemini <noreply@google.com>
 */

#include <pebble.h>
#include <stdlib.h> // Required for abs()

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

#define PERSIST_KEY_FONT 0
#define PERSIST_KEY_SCROLL 1
#define PERSIST_KEY_TIMEOUT_APP 2
#define PERSIST_KEY_TIMEOUT_DISC 3
#define PERSIST_KEY_TOUCH 4
#define PERSIST_KEY_THEME 5

#define ENABLE_VOLUME 1

// Platform detection for devices lacking official Clay support
#if defined(PBL_PLATFORM_GABBRO) || defined(PBL_PLATFORM_FLINT) || defined(PBL_PLATFORM_APLITE)
#define CLAY_SUPPORTED 0
#else
#define CLAY_SUPPORTED 1
#endif

#define RECT_SCALE_Y(val) (((val) * bounds.size.h) / 168)
#define RECT_SCALE_H(val) (((val) * bounds.size.h) / 168)
#define NATIVE_Y(rect_y, round_y) PBL_IF_ROUND_ELSE(round_y, RECT_SCALE_Y(rect_y))
#define NATIVE_H(rect_h, round_h) PBL_IF_ROUND_ELSE(round_h, RECT_SCALE_H(rect_h))

typedef enum {
  MODE_TRACK,
  MODE_ZONE,
  MODE_ERROR
} AppMode;

static AppMode s_mode = MODE_TRACK;
static Window *s_window;
static bool s_window_loaded = false;

// Configs
static int s_font_size = 1;
static int s_scroll_mode = 0; // 0 = Truncate, 1 = Marquee, 2 = Wrap
static int s_timeout_app_min = 0;
static int s_timeout_disc_min = 0;
static bool s_enable_touch = true;
static int s_theme = 0; // 0 = Dark, 1 = Light

// UI Layers
static BitmapLayer *s_logo_layer = NULL;
static GBitmap *s_logo_bitmap = NULL;
static TextLayer *s_track_layer = NULL;
static TextLayer *s_artist_layer = NULL;
static Layer *s_zone_layer = NULL;
static Layer *s_status_layer = NULL;

static GFont s_zone_font = NULL;

// Render Objects
static GPath *s_play_path = NULL;
static const GPoint s_large_play_points[] = {{-6, -12}, {-6, 12}, {12, 0}};
static const GPathInfo s_large_play_info = { .num_points = 3, .points = (GPoint *)s_large_play_points };

static const GPoint s_normal_play_points[] = {{-4, -8}, {-4, 8}, {8, 0}};
static const GPathInfo s_normal_play_info = { .num_points = 3, .points = (GPoint *)s_normal_play_points };

static const GPoint s_small_play_points[] = {{-3, -6}, {-3, 6}, {6, 0}};
static const GPathInfo s_small_play_info = { .num_points = 3, .points = (GPoint *)s_small_play_points };

static PropertyAnimation *s_marquee_anim = NULL;

#if ENABLE_VOLUME
static TextLayer *s_vol_layer = NULL;
static AppTimer *s_vol_revert_timer = NULL;
static char s_vol_buf[32];
static int s_volume = -1;

static AppTimer *s_vol_ignore_timer = NULL;
static bool s_ignore_vol_updates = false;
static AppTimer *s_vol_flash_timer = NULL;
static bool s_is_flashing_vol = false;
#endif

// Timers & State
static AppTimer *s_playpause_delay_timer = NULL;
static AppTimer *s_zone_revert_timer = NULL;
static AppTimer *s_btn_lock_timer = NULL;

// Touch Hold State
static AppTimer *s_touch_hold_timer = NULL;
static bool s_touch_held = false;

// Volume Gesture State
static AppTimer *s_vol_ready_timer = NULL;
static bool s_volume_mode_ready = false;
static bool s_volume_mode_active = false;
static int16_t s_last_vol_y = -1;

// App Timeout Timers
static AppTimer *s_app_idle_timer = NULL;
static AppTimer *s_disc_idle_timer = NULL;

// Play Optimistic Lock
static AppTimer *s_play_ignore_timer = NULL;
static bool s_ignore_play_updates = false;

static bool s_btns_locked = false;
static bool s_is_playing = false;
static bool s_is_fixed = false;
static bool s_app_in_focus = true;
static bool s_is_configuring = false;

// Touch Tracking
static int16_t s_touch_start_x = -1;
static int16_t s_touch_start_y = -1;

// Buffers
static char s_track_buf[128] = "";
static char s_artist_buf[128] = "";
static char s_zone_buf[64] = "";

static void update_ui();

static int get_tuple_int(Tuple *t) {
  if (!t) return -1;
  switch (t->length) {
    case 1: return t->value->int8;
    case 2: return t->value->int16;
    case 4: return t->value->int32;
    default: return t->value->int32;
  }
}

// --- IDLE TIMEOUT ENGINE ---
static void exit_app_cb(void *data) {
  window_stack_pop_all(true);
}

static void mark_user_interaction() {
  if (s_app_idle_timer) { app_timer_cancel(s_app_idle_timer); s_app_idle_timer = NULL; }
  if (s_timeout_app_min > 0 && s_mode != MODE_ERROR && !s_is_configuring) {
    s_app_idle_timer = app_timer_register(s_timeout_app_min * 60000, exit_app_cb, NULL);
  }
}

static void start_disconnect_timer() {
  if (s_app_idle_timer) { app_timer_cancel(s_app_idle_timer); s_app_idle_timer = NULL; }
  if (s_disc_idle_timer) { app_timer_cancel(s_disc_idle_timer); s_disc_idle_timer = NULL; }
  if (s_timeout_disc_min > 0 && !s_is_configuring) {
    s_disc_idle_timer = app_timer_register(s_timeout_disc_min * 60000, exit_app_cb, NULL);
  }
}

static void bluetooth_callback(bool connected) {
  if (!connected) {
    if (s_mode != MODE_ERROR) {
      s_mode = MODE_ERROR;
      start_disconnect_timer();
      update_ui();
    }
  }
}

// --- SYSTEM FOCUS HANDLER ---
static void focus_handler(bool in_focus) {
  s_app_in_focus = in_focus;
}

// --- NATIVE OUTBOX DISPATCHER ---
static void send_command(char *cmd) {
  if (!s_window_loaded) return;

  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (result == APP_MSG_OK) {
    dict_write_cstring(iter, KEY_COMMAND, cmd);
    result = app_message_outbox_send();

    if (result != APP_MSG_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox dispatch failed! Kernel Code: %d", (int)result);
    } else {
      APP_LOG(APP_LOG_LEVEL_INFO, "Dispatched command upstream: %s", cmd);
    }
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox allocation failed! Error Code: %d", (int)result);
  }
}

static void btn_unlock_cb(void *data) {
  s_btns_locked = false;
  s_btn_lock_timer = NULL;
}

static void lock_buttons_temporarily(int delay_ms) {
  s_btns_locked = true;
  if (s_btn_lock_timer) app_timer_cancel(s_btn_lock_timer);
  s_btn_lock_timer = app_timer_register(delay_ms, btn_unlock_cb, NULL);
}

#if ENABLE_VOLUME
static void vol_flash_cb(void *data) {
  s_is_flashing_vol = false;
  s_vol_flash_timer = NULL;
  update_ui();
}

static void flash_volume_ms(int ms) {
  s_is_flashing_vol = true;
  if (s_vol_flash_timer) app_timer_cancel(s_vol_flash_timer);
  s_vol_flash_timer = app_timer_register(ms, vol_flash_cb, NULL);
  update_ui();
}

static void cancel_vol_flash() {
  s_is_flashing_vol = false;
  if (s_vol_flash_timer) {
    app_timer_cancel(s_vol_flash_timer);
    s_vol_flash_timer = NULL;
  }
}
#endif

static void safe_set_text(TextLayer *layer, char *text) {
  if (s_window_loaded && layer && text) text_layer_set_text(layer, text);
}

static void apply_fonts() {
  if (!s_track_layer || !s_artist_layer || !s_zone_layer) return;

  if (s_play_path) {
    gpath_destroy(s_play_path);
    s_play_path = NULL;
  }

  if (s_font_size == 2) {
    text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
    s_zone_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    s_play_path = gpath_create(&s_large_play_info);
  } else if (s_font_size == 1) {
    text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
    s_zone_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    s_play_path = gpath_create(&s_normal_play_info);
  } else {
    text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    s_zone_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    s_play_path = gpath_create(&s_small_play_info);
  }

  GTextOverflowMode overflow_mode = (s_scroll_mode == 2) ? GTextOverflowModeWordWrap : GTextOverflowModeTrailingEllipsis;
  text_layer_set_overflow_mode(s_track_layer, overflow_mode);
  text_layer_set_overflow_mode(s_artist_layer, overflow_mode);

  if (s_zone_layer) layer_mark_dirty(s_zone_layer);
}

static void stop_marquee() {
  if (s_marquee_anim) {
    animation_unschedule(property_animation_get_animation(s_marquee_anim));
    property_animation_destroy(s_marquee_anim);
    s_marquee_anim = NULL;
  }
  if (s_track_layer && s_window_loaded) {
    Layer *root = window_get_root_layer(s_window);
    GRect bounds = layer_get_bounds(root);
    layer_set_frame(text_layer_get_layer(s_track_layer), GRect(0, NATIVE_Y(24, 28), bounds.size.w, NATIVE_H(52, 52)));
    text_layer_set_text_alignment(s_track_layer, GTextAlignmentCenter);
  }
}

static void start_marquee() {
  stop_marquee();
  if (s_scroll_mode != 1 || !s_window_loaded || !s_track_layer) return;

  const char* text = text_layer_get_text(s_track_layer);
  if (!text || strlen(text) == 0) return;

  GFont font;
  if (s_font_size == 2) font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  else if (s_font_size == 1) font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  else font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  Layer *root = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root);

  GSize text_size = graphics_text_layout_get_content_size(text, font, GRect(0, 0, 2000, 60), GTextOverflowModeWordWrap, GTextAlignmentLeft);

  if (text_size.w > bounds.size.w) {
    text_layer_set_text_alignment(s_track_layer, GTextAlignmentLeft);
    Layer *t_layer = text_layer_get_layer(s_track_layer);

    GRect start = GRect(bounds.size.w, NATIVE_Y(24, 28), text_size.w + 20, NATIVE_H(52, 52));
    GRect end = GRect(-text_size.w - 20, NATIVE_Y(24, 28), text_size.w + 20, NATIVE_H(52, 52));

    s_marquee_anim = property_animation_create_layer_frame(t_layer, &start, &end);
    Animation *anim = property_animation_get_animation(s_marquee_anim);

    int duration = (bounds.size.w + text_size.w + 40) * 20;
    animation_set_duration(anim, duration);
    animation_set_curve(anim, AnimationCurveLinear);
    animation_set_play_count(anim, ANIMATION_PLAY_COUNT_INFINITE);

    animation_schedule(anim);
  }
}

static void update_ui() {
  if (!s_window_loaded) return;

  if (s_mode == MODE_ERROR) {
    stop_marquee();
    safe_set_text(s_track_layer, "Bridge Not Found");
    safe_set_text(s_artist_layer, "Press SELECT to retry");
    if (s_zone_layer) layer_mark_dirty(s_zone_layer);

    if (s_status_layer) layer_set_hidden(s_status_layer, true);
    #if ENABLE_VOLUME
    if (s_vol_layer) layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
    #endif
    return;
  }

  if (s_status_layer) {
    layer_set_hidden(s_status_layer, false);
    layer_mark_dirty(s_status_layer);
  }

  safe_set_text(s_track_layer, s_track_buf);

  if (strcmp(s_track_buf, "No Core") == 0) {
    safe_set_text(s_artist_layer, "Is the extension enabled?");
  } else {
    safe_set_text(s_artist_layer, s_artist_buf);
  }

  if (s_zone_layer) {
    layer_mark_dirty(s_zone_layer);
  }

  #if ENABLE_VOLUME
  if (s_vol_layer) {
    if (s_is_flashing_vol) {
      if (s_is_fixed) snprintf(s_vol_buf, sizeof(s_vol_buf), "Fixed Vol");
      else if (s_volume == -1) snprintf(s_vol_buf, sizeof(s_vol_buf), "Vol: --");
      else snprintf(s_vol_buf, sizeof(s_vol_buf), "Vol: %d", s_volume);

      text_layer_set_text(s_vol_layer, s_vol_buf);
      layer_set_hidden(text_layer_get_layer(s_vol_layer), false);
    } else {
      layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
    }
  }
  #endif
}

static void zone_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_window_loaded || s_mode == MODE_ERROR) return;

  GRect bounds = layer_get_bounds(layer);
  if (!s_zone_font || strlen(s_zone_buf) == 0) return;

  GSize text_size = graphics_text_layout_get_content_size(s_zone_buf, s_zone_font, GRect(0, 0, bounds.size.w - 12, bounds.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);

  int padding_x = 6;
  int padding_y_total = 4;

  int box_width = text_size.w + (padding_x * 2);
  if (box_width > bounds.size.w) box_width = bounds.size.w;

  int box_height = text_size.h + padding_y_total;
  if (box_height > bounds.size.h) box_height = bounds.size.h;

  int box_x = (bounds.size.w - box_width) / 2;
  int box_y = (bounds.size.h - box_height) / 2;

  GRect box_rect = GRect(box_x, box_y, box_width, box_height);
  GRect text_rect = GRect(box_x + padding_x, box_y - 2, text_size.w, box_height + 4);

  GColor bg_color = s_theme == 1 ? GColorWhite : GColorBlack;
  GColor fg_color = s_theme == 1 ? GColorBlack : GColorWhite;

  if (s_mode == MODE_ZONE) {
    graphics_context_set_fill_color(ctx, fg_color);
    graphics_fill_rect(ctx, box_rect, 3, GCornersAll);
    graphics_context_set_text_color(ctx, bg_color);
  } else {
    graphics_context_set_stroke_color(ctx, fg_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_round_rect(ctx, box_rect, 3);
    graphics_context_set_text_color(ctx, fg_color);
  }

  graphics_draw_text(ctx, s_zone_buf, s_zone_font, text_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void zone_revert_callback(void *data) {
  s_zone_revert_timer = NULL;
  if (s_mode == MODE_ZONE) { s_mode = MODE_TRACK; update_ui(); }
}

static void reset_zone_timer() {
  if (s_zone_revert_timer) app_timer_cancel(s_zone_revert_timer);
  s_zone_revert_timer = app_timer_register(8000, zone_revert_callback, NULL);
}

static void cancel_zone_timer() {
  if (s_zone_revert_timer) { app_timer_cancel(s_zone_revert_timer); s_zone_revert_timer = NULL; }
}

#if ENABLE_VOLUME
static void vol_ignore_cb(void *data) {
  s_ignore_vol_updates = false;
  s_vol_ignore_timer = NULL;
}

static void lock_volume_updates() {
  s_ignore_vol_updates = true;
  if (s_vol_ignore_timer) app_timer_cancel(s_vol_ignore_timer);
  s_vol_ignore_timer = app_timer_register(1500, vol_ignore_cb, NULL);
}
#endif

// --- OPTIMISTIC PLAY/PAUSE UI TRIGGER ---
static void play_ignore_cb(void *data) {
  s_ignore_play_updates = false;
  s_play_ignore_timer = NULL;
}

static void lock_play_updates() {
  s_ignore_play_updates = true;
  if (s_play_ignore_timer) app_timer_cancel(s_play_ignore_timer);
  s_play_ignore_timer = app_timer_register(2000, play_ignore_cb, NULL);
}

static void send_playpause_cb(void *data) {
  s_playpause_delay_timer = NULL;
  send_command("playpause");
}

static void trigger_optimistic_playpause() {
  s_is_playing = !s_is_playing;
  lock_play_updates();
  if (s_status_layer) layer_mark_dirty(s_status_layer);
  vibes_short_pulse();
  if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
  s_playpause_delay_timer = app_timer_register(100, send_playpause_cb, NULL);
}

// --- BUTTON CONTROLS ---
static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  mark_user_interaction();

  #if ENABLE_VOLUME
  if (s_is_flashing_vol) {
    cancel_vol_flash();
    update_ui();
    return;
  }
  #endif

  if (s_mode == MODE_ZONE) {
    cancel_zone_timer();
    s_mode = MODE_TRACK;
    update_ui();
  } else {
    window_stack_pop(true);
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  mark_user_interaction();
  if (s_mode == MODE_ERROR) return;

  if (s_mode == MODE_TRACK) {
    #if ENABLE_VOLUME
    if (!s_is_fixed) {
      if (s_volume != -1) { s_volume += 2; if (s_volume > 100) s_volume = 100; }
      send_command("vol_up");
      lock_volume_updates();
      flash_volume_ms(2000);
    } else {
      flash_volume_ms(500);
    }
    #endif
  } else if (s_mode == MODE_ZONE) {
    if (s_btns_locked) return;
    reset_zone_timer();
    stop_marquee();

    send_command("prev_zone");
    lock_buttons_temporarily(300);
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  mark_user_interaction();
  if (s_mode == MODE_ERROR) return;

  if (s_mode == MODE_TRACK) {
    #if ENABLE_VOLUME
    if (!s_is_fixed) {
      if (s_volume != -1) { s_volume -= 2; if (s_volume < 0) s_volume = 0; }
      send_command("vol_down");
      lock_volume_updates();
      flash_volume_ms(2000);
    } else {
      flash_volume_ms(500);
    }
    #endif
  } else if (s_mode == MODE_ZONE) {
    if (s_btns_locked) return;
    reset_zone_timer();
    stop_marquee();

    send_command("next_zone");
    lock_buttons_temporarily(300);
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  mark_user_interaction();
  if (s_mode == MODE_ERROR) {
    safe_set_text(s_track_layer, "Retrying...");
    safe_set_text(s_artist_layer, "Please wait...");
    send_command("retry_connection");
    return;
  }

  if (s_btns_locked && s_mode == MODE_ZONE) return;

  #if ENABLE_VOLUME
  cancel_vol_flash();
  #endif

  if (s_mode == MODE_TRACK) {
    s_mode = MODE_ZONE;
    reset_zone_timer();
  }
  else if (s_mode == MODE_ZONE) {
    cancel_zone_timer();
    send_command("status");
    s_mode = MODE_TRACK;
  }

  update_ui();
}

static void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  mark_user_interaction();
  if (s_mode == MODE_ERROR || s_btns_locked) return;
  vibes_short_pulse();
  send_command("previous");
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  mark_user_interaction();
  if (s_mode == MODE_ERROR || s_btns_locked) return;
  vibes_short_pulse();
  send_command("next");
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  mark_user_interaction();
  if (s_mode == MODE_ERROR || s_btns_locked) return;
  trigger_optimistic_playpause();
  if (s_mode == MODE_ZONE) reset_zone_timer();
}

static void select_double_click_handler(ClickRecognizerRef recognizer, void *context) {
  mark_user_interaction();
  if (s_mode == MODE_ERROR || s_btns_locked) return;

  vibes_long_pulse();
  stop_marquee();
  safe_set_text(s_track_layer, "Pausing All...");
  safe_set_text(s_artist_layer, "");

  s_track_buf[0] = '\0';
  s_artist_buf[0] = '\0';

  send_command("pause_all");
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

// --- TOUCH ENGINE DISPATCHER ---
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
  mark_user_interaction();
  if (s_mode == MODE_ERROR || s_btns_locked || !s_enable_touch || !s_app_in_focus) return;

  if (event->type == TouchEvent_Touchdown) {
    s_touch_start_x = event->x;
    s_touch_start_y = event->y;
    s_last_vol_y = event->y;
    s_touch_held = false;
    s_volume_mode_ready = false;
    s_volume_mode_active = false;

    #if ENABLE_VOLUME
    if (s_vol_flash_timer) {
      app_timer_cancel(s_vol_flash_timer);
      s_vol_flash_timer = NULL;
    }
    #endif

    if (s_touch_hold_timer) app_timer_cancel(s_touch_hold_timer);
    // Initiate 600ms hold timer
    s_touch_hold_timer = app_timer_register(600, touch_hold_cb, NULL);

    if (s_vol_ready_timer) app_timer_cancel(s_vol_ready_timer);
    // Initiate 400ms volume drag ready timer
    s_vol_ready_timer = app_timer_register(400, vol_drag_ready_cb, NULL);
  }
  else if (event->type == TouchEvent_PositionUpdate) {
    int16_t current_x = event->x;
    int16_t current_y = event->y;
    int16_t dx = abs(current_x - s_touch_start_x);
    int16_t dy = abs(current_y - s_touch_start_y);

    // Detect volume gesture drag
    if (s_volume_mode_ready && dy > 15 && dy > dx * 2) {
      if (!s_volume_mode_active) {
        s_volume_mode_active = true;

        if (s_touch_hold_timer) {
          app_timer_cancel(s_touch_hold_timer);
          s_touch_hold_timer = NULL;
        }
        s_touch_held = false; // Abort any pending pause-all intent

        vibes_double_pulse();
      }

      int16_t step_dy = current_y - s_last_vol_y;
      if (step_dy <= -10 || step_dy >= 10) {
        #if ENABLE_VOLUME
        if (!s_is_fixed) {
          if (step_dy <= -10) {
            if (s_volume != -1) { s_volume += 2; if (s_volume > 100) s_volume = 100; }
            send_command("vol_up");
            lock_volume_updates();
          } else {
            if (s_volume != -1) { s_volume -= 2; if (s_volume < 0) s_volume = 0; }
            send_command("vol_down");
            lock_volume_updates();
          }
          flash_volume_ms(3000);
        } else {
          flash_volume_ms(1000);
        }
        #endif
        s_last_vol_y = current_y;
      }
    }
    // Standard swipe detection overrides the hold functionality
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
    // Cancel timers as finger has left the screen
    if (s_touch_hold_timer) {
      app_timer_cancel(s_touch_hold_timer);
      s_touch_hold_timer = NULL;
    }
    if (s_vol_ready_timer) {
      app_timer_cancel(s_vol_ready_timer);
      s_vol_ready_timer = NULL;
    }

    if (s_volume_mode_active) {
      s_volume_mode_active = false;
      s_touch_start_x = -1;
      s_touch_start_y = -1;
      #if ENABLE_VOLUME
      flash_volume_ms(3000);
      #endif
      return;
    }

    if (s_touch_held) {
      s_touch_held = false;
      s_touch_start_x = -1;
      s_touch_start_y = -1;
      stop_marquee();
      safe_set_text(s_track_layer, "Pausing All...");
      safe_set_text(s_artist_layer, "");
      s_track_buf[0] = '\0';
      s_artist_buf[0] = '\0';
      send_command("pause_all");
      return;
    }

    if (s_touch_start_x != -1 && s_touch_start_y != -1) {
      int16_t delta_x = event->x - s_touch_start_x;
      int16_t delta_y = event->y - s_touch_start_y;
      int16_t abs_dx = abs(delta_x);
      int16_t abs_dy = abs(delta_y);

      #if ENABLE_VOLUME
      // Retain the ability to continuously adjust volume via rapid vertical swipes while the volume is on screen
      if (s_is_flashing_vol) {
        if (!s_is_fixed && abs_dy > 20 && abs_dy > abs_dx) {
          vibes_short_pulse();
          if (delta_y < 0) {
            if (s_volume != -1) { s_volume += 2; if (s_volume > 100) s_volume = 100; }
            send_command("vol_up");
            lock_volume_updates();
          } else {
            if (s_volume != -1) { s_volume -= 2; if (s_volume < 0) s_volume = 0; }
            send_command("vol_down");
            lock_volume_updates();
          }
        }
        flash_volume_ms(3000);
      }
      else
        #endif
        // Horizontal Swipe Detection
        if (abs_dx > 30 && abs_dx > abs_dy) {
          if (s_mode == MODE_TRACK) {
            vibes_short_pulse();
            if (delta_x > 0) {
              send_command("previous");
            } else {
              send_command("next");
            }
            lock_buttons_temporarily(300);
          }
        }
        // Vertical Swipe Detection
        else if (abs_dy > 30 && abs_dy > abs_dx) {
          vibes_short_pulse();
          reset_zone_timer();
          stop_marquee();

          if (delta_y > 0) {
            send_command("prev_zone");
          } else {
            send_command("next_zone");
          }
          lock_buttons_temporarily(300);
        }
        // Instant Tap Detection
        else if (abs_dx < 10 && abs_dy < 10) {
          if (s_mode == MODE_TRACK) {
            trigger_optimistic_playpause();
          } else if (s_mode == MODE_ZONE) {
            reset_zone_timer();
          }
        }
    }

    // Reset tracking sequence memory
    s_touch_start_x = -1;
    s_touch_start_y = -1;
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  mark_user_interaction();
  if (s_mode == MODE_ERROR || s_btns_locked || !s_enable_touch || !s_app_in_focus) return;

  if (axis == ACCEL_AXIS_Z) {
    if (s_mode == MODE_TRACK) {
      trigger_optimistic_playpause();
    } else if (s_mode == MODE_ZONE) {
      reset_zone_timer();
    }
  }
}

static void status_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_window_loaded || s_mode == MODE_ERROR) return;
  GRect bounds = layer_get_bounds(layer);

  GColor main_color = s_theme == 1 ? GColorBlack : GColorWhite;

  if (s_mode == MODE_TRACK) {
    graphics_context_set_fill_color(ctx, main_color);
    if (s_is_playing) {
      if (s_font_size == 2) {
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 - 8, bounds.size.h/2 - 12, 6, 24), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 + 2, bounds.size.h/2 - 12, 6, 24), 0, GCornerNone);
      } else if (s_font_size == 1) {
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 - 6, bounds.size.h/2 - 8, 4, 16), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 + 2, bounds.size.h/2 - 8, 4, 16), 0, GCornerNone);
      } else {
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 - 4, bounds.size.h/2 - 6, 3, 12), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 + 1, bounds.size.h/2 - 6, 3, 12), 0, GCornerNone);
      }
    } else {
      if (s_play_path) {
        gpath_move_to(s_play_path, GPoint(bounds.size.w/2, bounds.size.h/2));
        gpath_draw_filled(ctx, s_play_path);
      }
    }
  } else {
    graphics_context_set_text_color(ctx, main_color);
    const char* mode_text = "";

    if (s_mode == MODE_ZONE) mode_text = "Select Zone";

    graphics_draw_text(ctx, mode_text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(0, -2, bounds.size.w, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// --- LOGGING HANDLERS ---
static void outbox_sent_handler(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "AppMessage cleared link layer successfully.");
}

static void outbox_failed_handler(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Upstream AppMessage transmission dropped. Reason code: %d", (int)reason);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  if (!s_window_loaded) return;
  Tuple *t;

  // 1. Process all configuration settings first to ensure UI updates arent blocked by network errors

  if ((t = dict_find(iterator, KEY_IS_CONFIGURING))) {
    bool is_config = (get_tuple_int(t) == 1);

    #if !CLAY_SUPPORTED
    // If the custom HTML fallback lacks Clay's lifecycle events, ignore the configuring state
    // so the watchapp continues to handle timeouts and idle states correctly.
    is_config = false;
    #endif

    if (s_is_configuring != is_config) {
      s_is_configuring = is_config;
      if (s_is_configuring) {
        if (s_app_idle_timer) { app_timer_cancel(s_app_idle_timer); s_app_idle_timer = NULL; }
        if (s_disc_idle_timer) { app_timer_cancel(s_disc_idle_timer); s_disc_idle_timer = NULL; }
      } else {
        if (s_mode == MODE_ERROR) {
          start_disconnect_timer();
        } else {
          mark_user_interaction();
        }
      }
    }
  }

  if ((t = dict_find(iterator, KEY_THEME))) {
    int requested_theme = get_tuple_int(t);
    if (s_theme != requested_theme) {
      s_theme = requested_theme;
      persist_write_int(PERSIST_KEY_THEME, s_theme);
      if (s_window_loaded) {
        window_set_background_color(s_window, s_theme == 1 ? GColorWhite : GColorBlack);
        GColor text_color = s_theme == 1 ? GColorBlack : GColorWhite;
        text_layer_set_text_color(s_track_layer, text_color);
        text_layer_set_text_color(s_artist_layer, text_color);
        #if ENABLE_VOLUME
        if (s_vol_layer) {
          text_layer_set_background_color(s_vol_layer, s_theme == 1 ? GColorWhite : GColorBlack);
          text_layer_set_text_color(s_vol_layer, text_color);
        }
        #endif

        if (s_logo_bitmap) {
          gbitmap_destroy(s_logo_bitmap);
        }
        s_logo_bitmap = gbitmap_create_with_resource(s_theme == 1 ? RESOURCE_ID_IMAGE_LOGO_LIGHT : RESOURCE_ID_IMAGE_LOGO_DARK);
        if (s_logo_layer) {
          bitmap_layer_set_bitmap(s_logo_layer, s_logo_bitmap);
        }

        update_ui();
      }
    }
  }

  if ((t = dict_find(iterator, KEY_FONT_SIZE))) {
    int requested_size = get_tuple_int(t);
    if (s_font_size != requested_size) {
      s_font_size = requested_size;
      persist_write_int(PERSIST_KEY_FONT, s_font_size);
      apply_fonts();
      start_marquee();
      update_ui();
    }
  }

  if ((t = dict_find(iterator, KEY_SCROLL_TEXT))) {
    int requested_scroll = get_tuple_int(t);
    if (s_scroll_mode != requested_scroll) {
      s_scroll_mode = requested_scroll;
      persist_write_int(PERSIST_KEY_SCROLL, s_scroll_mode);
      apply_fonts(); // Refresh overflow mode based on wrap/truncate state
      if (s_scroll_mode == 1) {
        start_marquee();
      } else {
        stop_marquee();
      }
      update_ui();
    }
  }

  if ((t = dict_find(iterator, KEY_TIMEOUT_APP))) {
    int new_timeout = get_tuple_int(t);
    if (s_timeout_app_min != new_timeout) {
      s_timeout_app_min = new_timeout;
      persist_write_int(PERSIST_KEY_TIMEOUT_APP, s_timeout_app_min);
      mark_user_interaction();
    }
  }

  if ((t = dict_find(iterator, KEY_TIMEOUT_DISC))) {
    int new_timeout = get_tuple_int(t);
    if (s_timeout_disc_min != new_timeout) {
      s_timeout_disc_min = new_timeout;
      persist_write_int(PERSIST_KEY_TIMEOUT_DISC, s_timeout_disc_min);
      if (s_mode == MODE_ERROR) start_disconnect_timer();
    }
  }

  if ((t = dict_find(iterator, KEY_ENABLE_TOUCH))) {
    bool requested_touch = (get_tuple_int(t) == 1);
    if (s_enable_touch != requested_touch) {
      s_enable_touch = requested_touch;
      persist_write_bool(PERSIST_KEY_TOUCH, s_enable_touch);
    }
  }

  // 2. Now process the error check
  if ((t = dict_find(iterator, KEY_ERROR))) {
    if (get_tuple_int(t) == 1) {
      if (s_mode != MODE_ERROR) {
        s_mode = MODE_ERROR;
        start_disconnect_timer();
        update_ui();
      }
      return;
    } else {
      if (s_mode == MODE_ERROR) {
        s_mode = MODE_TRACK;
        if (s_disc_idle_timer) { app_timer_cancel(s_disc_idle_timer); s_disc_idle_timer = NULL; }
        mark_user_interaction();
        update_ui();
      }
    }
  }

  if (s_mode == MODE_ERROR) return;

  // 3. Process track and playback data
  if ((t = dict_find(iterator, KEY_ZONE_NAME))) {
    snprintf(s_zone_buf, sizeof(s_zone_buf), "%s", t->value->cstring);
    if (s_zone_layer) layer_mark_dirty(s_zone_layer);
  }

  if ((t = dict_find(iterator, KEY_TRACK))) {
    if (strcmp(s_track_buf, t->value->cstring) != 0) {
      snprintf(s_track_buf, sizeof(s_track_buf), "%s", t->value->cstring);

      #if ENABLE_VOLUME
      if (!s_is_flashing_vol) {
        safe_set_text(s_track_layer, s_track_buf);
        start_marquee();
      }
      #else
      safe_set_text(s_track_layer, s_track_buf);
      start_marquee();
      #endif

      if (strcmp(s_track_buf, "No Core") == 0) {
        safe_set_text(s_artist_layer, "Is the extension enabled?");
      } else {
        safe_set_text(s_artist_layer, s_artist_buf);
      }
    }
  }

  if ((t = dict_find(iterator, KEY_ARTIST))) {
    if (strcmp(s_artist_buf, t->value->cstring) != 0) {
      snprintf(s_artist_buf, sizeof(s_artist_buf), "%s", t->value->cstring);
      if (strcmp(s_track_buf, "No Core") == 0) {
        safe_set_text(s_artist_layer, "Is the extension enabled?");
      } else {
        safe_set_text(s_artist_layer, s_artist_buf);
      }
    }
  }

  if ((t = dict_find(iterator, KEY_IS_PLAYING))) {
    if (!s_ignore_play_updates) {
      bool is_playing = (get_tuple_int(t) == 1);
      if (s_is_playing != is_playing) {
        s_is_playing = is_playing;
        if (s_status_layer) layer_mark_dirty(s_status_layer);
      }
    }
  }

  #if ENABLE_VOLUME
  if ((t = dict_find(iterator, KEY_VOLUME_VAL))) {
    if (!s_ignore_vol_updates) {
      s_volume = get_tuple_int(t);
      if (s_is_flashing_vol) update_ui();
    }
  }
  #endif

  if ((t = dict_find(iterator, KEY_IS_FIXED))) s_is_fixed = (get_tuple_int(t) == 1);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, s_theme == 1 ? GColorWhite : GColorBlack);

  s_logo_bitmap = gbitmap_create_with_resource(s_theme == 1 ? RESOURCE_ID_IMAGE_LOGO_LIGHT : RESOURCE_ID_IMAGE_LOGO_DARK);
  s_logo_layer = bitmap_layer_create(GRect(0, NATIVE_Y(5, 12), bounds.size.w, NATIVE_H(35, 35)));
  bitmap_layer_set_background_color(s_logo_layer, GColorClear);
  bitmap_layer_set_bitmap(s_logo_layer, s_logo_bitmap);
  bitmap_layer_set_compositing_mode(s_logo_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_logo_layer, GAlignCenter);
  layer_add_child(root, bitmap_layer_get_layer(s_logo_layer));

  GColor text_color = s_theme == 1 ? GColorBlack : GColorWhite;

  s_track_layer = text_layer_create(GRect(0, NATIVE_Y(24, 28), bounds.size.w, NATIVE_H(52, 52)));
  text_layer_set_text(s_track_layer, "Loading...");
  text_layer_set_text_alignment(s_track_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_track_layer, GColorClear);
  text_layer_set_text_color(s_track_layer, text_color);
  layer_add_child(root, text_layer_get_layer(s_track_layer));

  s_status_layer = layer_create(GRect(0, NATIVE_Y(76, 80), bounds.size.w, NATIVE_H(16, 16)));
  layer_set_update_proc(s_status_layer, status_layer_update_proc);
  layer_add_child(root, s_status_layer);

  s_artist_layer = text_layer_create(GRect(0, NATIVE_Y(102, 106), bounds.size.w, NATIVE_H(44, 44)));
  text_layer_set_text_alignment(s_artist_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_artist_layer, GColorClear);
  text_layer_set_text_color(s_artist_layer, text_color);
  layer_add_child(root, text_layer_get_layer(s_artist_layer));

  s_zone_layer = layer_create(GRect(0, bounds.size.h - NATIVE_H(26, 32), bounds.size.w, NATIVE_H(26, 26)));
  layer_set_update_proc(s_zone_layer, zone_layer_update_proc);
  layer_add_child(root, s_zone_layer);

  #if ENABLE_VOLUME
  s_vol_layer = text_layer_create(GRect(0, NATIVE_Y(60, 64), bounds.size.w, NATIVE_H(48, 48)));
  text_layer_set_text(s_vol_layer, "Vol: --");
  text_layer_set_font(s_vol_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_vol_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_vol_layer, s_theme == 1 ? GColorWhite : GColorBlack);
  text_layer_set_text_color(s_vol_layer, text_color);
  layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
  layer_add_child(root, text_layer_get_layer(s_vol_layer));
  #endif

  apply_fonts();
  s_window_loaded = true;

  mark_user_interaction();
}

static void window_unload(Window *window) {
  s_window_loaded = false;

  if (s_play_path) { gpath_destroy(s_play_path); s_play_path = NULL; }
  if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
  if (s_btn_lock_timer) app_timer_cancel(s_btn_lock_timer);
  if (s_app_idle_timer) app_timer_cancel(s_app_idle_timer);
  if (s_disc_idle_timer) app_timer_cancel(s_disc_idle_timer);
  if (s_play_ignore_timer) app_timer_cancel(s_play_ignore_timer);
  if (s_touch_hold_timer) app_timer_cancel(s_touch_hold_timer);
  if (s_vol_ready_timer) app_timer_cancel(s_vol_ready_timer);

  stop_marquee();
  cancel_zone_timer();

  #if ENABLE_VOLUME
  if (s_vol_ignore_timer) app_timer_cancel(s_vol_ignore_timer);
  if (s_vol_flash_timer) app_timer_cancel(s_vol_flash_timer);
  text_layer_destroy(s_vol_layer);
  s_vol_layer = NULL;
  #endif

  text_layer_destroy(s_track_layer);
  text_layer_destroy(s_artist_layer);
  layer_destroy(s_zone_layer);
  layer_destroy(s_status_layer);
  bitmap_layer_destroy(s_logo_layer);
  gbitmap_destroy(s_logo_bitmap);
}

static void init(void) {
  if (persist_exists(PERSIST_KEY_FONT)) s_font_size = persist_read_int(PERSIST_KEY_FONT);
  if (persist_exists(PERSIST_KEY_SCROLL)) {
    // Read old boolean flags as integer values (false = 0, true = 1), mapping natively to Truncate or Marquee
    s_scroll_mode = persist_read_int(PERSIST_KEY_SCROLL);
  }
  if (persist_exists(PERSIST_KEY_TIMEOUT_APP)) s_timeout_app_min = persist_read_int(PERSIST_KEY_TIMEOUT_APP);
  if (persist_exists(PERSIST_KEY_TIMEOUT_DISC)) s_timeout_disc_min = persist_read_int(PERSIST_KEY_TIMEOUT_DISC);
  if (persist_exists(PERSIST_KEY_TOUCH)) s_enable_touch = persist_read_bool(PERSIST_KEY_TOUCH);
  if (persist_exists(PERSIST_KEY_THEME)) s_theme = persist_read_int(PERSIST_KEY_THEME);

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);

  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .will_focus = focus_handler,
    .did_focus = focus_handler
  });

  if (touch_service_is_enabled()) {
    touch_service_subscribe(touch_handler, NULL);
  } else {
    accel_tap_service_subscribe(accel_tap_handler);
  }

  connection_service_subscribe((ConnectionHandlers) { .pebble_app_connection_handler = bluetooth_callback });

  window_set_window_handlers(s_window, (WindowHandlers) { .load = window_load, .unload = window_unload });

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_outbox_sent(outbox_sent_handler);
  app_message_register_outbox_failed(outbox_failed_handler);

  // Use explicit allocation bounds to prevent OOM failure on aplite
  app_message_open(1024, 256);

  window_stack_push(s_window, true);
}

static void deinit(void) {
  app_focus_service_unsubscribe();

  if (touch_service_is_enabled()) {
    touch_service_unsubscribe();
  } else {
    accel_tap_service_unsubscribe();
  }

  connection_service_unsubscribe();
  window_destroy(s_window);

  app_message_deregister_callbacks();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
