/*
 * Pebble Roon Remote - AppMessage & Networking
 * Copyright (c) 2026 J_B
 */

#include <pebble.h>
#include "app_state.h"

#if defined(PBL_PLATFORM_GABBRO) || defined(PBL_PLATFORM_FLINT) || defined(PBL_PLATFORM_APLITE)
#define CLAY_SUPPORTED 0
#else
#define CLAY_SUPPORTED 1
#endif

static int get_tuple_int(Tuple *t) {
  if (!t) return -1;
  switch (t->length) {
    case 1: return t->value->int8;
    case 2: return t->value->int16;
    case 4: return t->value->int32;
    default: return t->value->int32;
  }
}

void comm_send_command(const char *cmd) {
  if (!g_state.window_loaded) return;

  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (result == APP_MSG_OK) {
    dict_write_cstring(iter, KEY_COMMAND, cmd);
    app_message_outbox_send();
  }
}

static void outbox_sent_handler(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "AppMessage cleared link layer successfully.");
}

static void outbox_failed_handler(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Upstream AppMessage transmission dropped. Reason code: %d", (int)reason);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  if (!g_state.window_loaded) return;
  Tuple *t;
  bool needs_update = false;
  bool layout_dirty = false;
  bool subs_dirty = false;

  if ((t = dict_find(iterator, KEY_IS_CONFIGURING))) {
    bool is_config = (get_tuple_int(t) == 1);
    #if !CLAY_SUPPORTED
    is_config = false;
    #endif

    if (g_state.is_configuring != is_config) {
      g_state.is_configuring = is_config;
      if (g_state.is_configuring) {
        app_cancel_idle_timers();
      } else {
        if (g_state.mode == MODE_ERROR) app_start_disconnect_timer();
        else app_mark_user_interaction();
      }
    }
  }

  if ((t = dict_find(iterator, KEY_THEME))) {
    int requested_theme = get_tuple_int(t);
    if (g_state.theme != requested_theme) {
      g_state.theme = requested_theme;
      persist_write_int(PERSIST_KEY_THEME, g_state.theme);
      ui_apply_theme();
      needs_update = true;
    }
  }

  if ((t = dict_find(iterator, KEY_ENABLE_WATCHFACE))) {
    bool new_wf = (get_tuple_int(t) == 1);
    if (g_state.enable_watchface != new_wf) {
      g_state.enable_watchface = new_wf;
      persist_write_bool(PERSIST_KEY_WATCHFACE, g_state.enable_watchface);
      layout_dirty = true;
      needs_update = true;
      subs_dirty = true;
    }
  }

  if ((t = dict_find(iterator, KEY_RESPECT_QUIET_TIME))) {
    bool new_qt = (get_tuple_int(t) == 1);
    if (g_state.respect_quiet_time != new_qt) {
      g_state.respect_quiet_time = new_qt;
      persist_write_bool(PERSIST_KEY_QUIET_TIME, g_state.respect_quiet_time);
      needs_update = true;
    }
  }

  if ((t = dict_find(iterator, KEY_TIMEOUT_TO_APP_WF))) {
    bool new_timeout_wf = (get_tuple_int(t) == 1);
    if (g_state.timeout_to_app_wf != new_timeout_wf) {
      g_state.timeout_to_app_wf = new_timeout_wf;
      persist_write_bool(PERSIST_KEY_TIMEOUT_DEST, g_state.timeout_to_app_wf);
    }
  }

  if ((t = dict_find(iterator, KEY_SUPPRESS_GESTURE_QUIET))) {
    bool new_suppress_wf = (get_tuple_int(t) == 1);
    if (g_state.suppress_gesture_quiet != new_suppress_wf) {
      g_state.suppress_gesture_quiet = new_suppress_wf;
      persist_write_bool(PERSIST_KEY_SUPPRESS_GESTURE_QT, g_state.suppress_gesture_quiet);
    }
  }

  if ((t = dict_find(iterator, KEY_ENABLE_ACCEL_PLAYPAUSE))) {
    bool new_accel = (get_tuple_int(t) == 1);
    if (g_state.enable_accel_playpause != new_accel) {
      g_state.enable_accel_playpause = new_accel;
      persist_write_bool(PERSIST_KEY_ACCEL, g_state.enable_accel_playpause);
      subs_dirty = true;
    }
  }

  if ((t = dict_find(iterator, KEY_ENABLE_FLICK_VIBES))) {
    bool new_vibes = (get_tuple_int(t) == 1);
    if (g_state.enable_flick_vibes != new_vibes) {
      g_state.enable_flick_vibes = new_vibes;
      persist_write_bool(PERSIST_KEY_FLICK_VIBES, g_state.enable_flick_vibes);
    }
  }

  if ((t = dict_find(iterator, KEY_FONT_SIZE))) {
    int requested_size = get_tuple_int(t);
    if (g_state.font_size != requested_size) {
      g_state.font_size = requested_size;
      persist_write_int(PERSIST_KEY_FONT, g_state.font_size);
      ui_stop_marquee();
      ui_apply_fonts();
      layout_dirty = true;
      needs_update = true;
    }
  }

  if ((t = dict_find(iterator, KEY_SCROLL_TEXT))) {
    int requested_scroll = get_tuple_int(t);
    if (g_state.scroll_mode != requested_scroll) {
      g_state.scroll_mode = requested_scroll;
      persist_write_int(PERSIST_KEY_SCROLL, g_state.scroll_mode);
      ui_stop_marquee();
      ui_apply_fonts();
      layout_dirty = true;
      needs_update = true;
    }
  }

  if ((t = dict_find(iterator, KEY_TIMEOUT_APP))) {
    int new_timeout = get_tuple_int(t);
    if (g_state.timeout_app_min != new_timeout) {
      g_state.timeout_app_min = new_timeout;
      persist_write_int(PERSIST_KEY_TIMEOUT_APP, g_state.timeout_app_min);
      app_mark_user_interaction();
    }
  }

  if ((t = dict_find(iterator, KEY_TIMEOUT_DISC))) {
    int new_timeout = get_tuple_int(t);
    if (g_state.timeout_disc_min != new_timeout) {
      g_state.timeout_disc_min = new_timeout;
      persist_write_int(PERSIST_KEY_TIMEOUT_DISC, g_state.timeout_disc_min);
      if (g_state.mode == MODE_ERROR) app_start_disconnect_timer();
    }
  }

  if ((t = dict_find(iterator, KEY_ENABLE_TOUCH))) {
    bool requested_touch = (get_tuple_int(t) == 1);
    if (g_state.enable_touch != requested_touch) {
      g_state.enable_touch = requested_touch;
      persist_write_bool(PERSIST_KEY_TOUCH, g_state.enable_touch);
    }
  }

  if ((t = dict_find(iterator, KEY_ERROR))) {
    if (get_tuple_int(t) == 1) {
      if (g_state.mode != MODE_ERROR) {
        g_state.mode = MODE_ERROR;
        app_start_disconnect_timer();
        needs_update = true;
      }
      if (needs_update) ui_update();
      return;
    } else {
      if (g_state.mode == MODE_ERROR) {
        g_state.mode = MODE_TRACK;
        app_mark_user_interaction();
        needs_update = true;
        if (g_state.scroll_mode == 1) ui_start_marquee();
      }
    }
  }

  if (g_state.mode == MODE_ERROR) return;

  if ((t = dict_find(iterator, KEY_ZONE_NAME))) {
    snprintf(g_state.zone_buf, sizeof(g_state.zone_buf), "%s", t->value->cstring);
    needs_update = true;
  }

  if ((t = dict_find(iterator, KEY_TRACK))) {
    if (strcmp(g_state.track_buf, t->value->cstring) != 0) {
      snprintf(g_state.track_buf, sizeof(g_state.track_buf), "%s", t->value->cstring);
      layout_dirty = true;
      needs_update = true;
    }
  }

  if ((t = dict_find(iterator, KEY_ARTIST))) {
    if (strcmp(g_state.artist_buf, t->value->cstring) != 0) {
      snprintf(g_state.artist_buf, sizeof(g_state.artist_buf), "%s", t->value->cstring);
      layout_dirty = true;
      needs_update = true;
    }
  }

  if ((t = dict_find(iterator, KEY_IS_PLAYING))) {
    if (!g_state.ignore_play_updates) {
      bool is_playing = (get_tuple_int(t) == 1);
      if (g_state.is_playing != is_playing) {
        g_state.is_playing = is_playing;
        needs_update = true;

        if (g_state.is_playing && g_state.scroll_mode == 1) {
          ui_start_marquee();
        } else if (!g_state.is_playing) {
          ui_stop_marquee();
        }
      }
    }
  }

  #if ENABLE_VOLUME
  if ((t = dict_find(iterator, KEY_VOLUME_VAL))) {
    if (!g_state.ignore_vol_updates) {
      g_state.volume = get_tuple_int(t);
      if (g_state.is_flashing_vol) needs_update = true;
    }
  }
  #endif

  if ((t = dict_find(iterator, KEY_IS_FIXED))) g_state.is_fixed = (get_tuple_int(t) == 1);

  if (subs_dirty) {
    app_update_subscriptions();
  }

  if (layout_dirty) {
    ui_stop_marquee();
    ui_apply_layout();
    if (g_state.scroll_mode == 1) ui_start_marquee();
  }

  if (needs_update) {
    ui_update();
  }
}

void comm_init(void) {
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_outbox_sent(outbox_sent_handler);
  app_message_register_outbox_failed(outbox_failed_handler);
  app_message_open(512, 128);
}

void comm_deinit(void) {
  app_message_deregister_callbacks();
}
