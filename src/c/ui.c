/*
 * Pebble Roon Remote - UI Engine & Layers
 * Copyright (c) 2026 J_B
 */

#include <pebble.h>
#include "app_state.h"
#include "watchface.h"

// Safe UI Scaling Macros requiring explicit height boundary
#define RECT_SCALE_Y(val, h) (((val) * (h)) / 168)
#define RECT_SCALE_H(val, h) (((val) * (h)) / 168)
#define ROUND_SCALE_Y(val, h) (((val) * (h)) / 180)
#define ROUND_SCALE_H(val, h) (((val) * (h)) / 180)
#define NATIVE_Y(rect_y, round_y, h) PBL_IF_ROUND_ELSE(ROUND_SCALE_Y(round_y, h), RECT_SCALE_Y(rect_y, h))
#define NATIVE_H(rect_h, round_h, h) PBL_IF_ROUND_ELSE(ROUND_SCALE_H(round_h, h), RECT_SCALE_H(rect_h, h))

static BitmapLayer *s_logo_layer = NULL;
static GBitmap *s_logo_bitmap = NULL;
static TextLayer *s_track_layer = NULL;
static TextLayer *s_artist_layer = NULL;
static TextLayer *s_back_layer = NULL;
static Layer *s_zone_layer = NULL;
static Layer *s_status_layer = NULL;

static GFont s_custom_font_42;
static GFont s_zone_font = NULL;

static GPath *s_play_path = NULL;
static const GPoint s_large_play_points[] = {{-6, -12}, {-6, 12}, {12, 0}};
static const GPathInfo s_large_play_info = { .num_points = 3, .points = (GPoint *)s_large_play_points };

static const GPoint s_normal_play_points[] = {{-4, -8}, {-4, 8}, {8, 0}};
static const GPathInfo s_normal_play_info = { .num_points = 3, .points = (GPoint *)s_normal_play_points };

static const GPoint s_small_play_points[] = {{-3, -6}, {-3, 6}, {6, 0}};
static const GPathInfo s_small_play_info = { .num_points = 3, .points = (GPoint *)s_small_play_points };

static Animation *s_marquee_spawn_anim = NULL;

#if ENABLE_VOLUME
static TextLayer *s_vol_layer = NULL;
static AppTimer *s_vol_flash_timer = NULL;
#endif

// --- INTERNAL HELPERS ---
static void safe_set_text(TextLayer *layer, char *text) {
  if (g_state.window_loaded && layer && text) text_layer_set_text(layer, text);
}

static void recalculate_gesture_centering(GRect bounds) {
  if (s_marquee_spawn_anim) return;

  if (g_state.enable_watchface && g_state.gesture_mode_active) {
    int16_t logo_bottom = NATIVE_Y(5, 12, bounds.size.h) + NATIVE_H(35, 35, bounds.size.h);
    int16_t status_y = (bounds.size.h / 2) - 16;
    int16_t status_bottom = status_y + 32;
    int16_t zone_y = bounds.size.h - NATIVE_H(26, 32, bounds.size.h);
    int16_t max_width = PBL_IF_ROUND_ELSE(bounds.size.w - 24, bounds.size.w);
    int16_t offset_x = (bounds.size.w - max_width) / 2;

    GTextOverflowMode overflow = GTextOverflowModeTrailingEllipsis;

    const char *track_text = text_layer_get_text(s_track_layer);
    GFont track_font = g_state.font_size == 2 ? fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD) :
    g_state.font_size == 1 ? fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD) :
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

    int line_h_t = (g_state.font_size == 2) ? 28 : (g_state.font_size == 1) ? 24 : 18;
    int16_t t_h = line_h_t + 8;

    if (g_state.scroll_mode != 1) {
      GSize t_size = graphics_text_layout_get_content_size(track_text, track_font, GRect(0, 0, max_width, 1000), overflow, GTextAlignmentCenter);
      t_h = t_size.h + 8;
    }

    if (t_h > (status_y - logo_bottom)) t_h = status_y - logo_bottom;
    int16_t t_y = logo_bottom + ((status_y - logo_bottom) - t_h) / 2;
    layer_set_frame(text_layer_get_layer(s_track_layer), GRect(offset_x, t_y, max_width, t_h));

    const char *artist_text = text_layer_get_text(s_artist_layer);
    GFont artist_font = g_state.font_size == 2 ? fonts_get_system_font(FONT_KEY_GOTHIC_28) :
    g_state.font_size == 1 ? fonts_get_system_font(FONT_KEY_GOTHIC_24) :
    fonts_get_system_font(FONT_KEY_GOTHIC_18);

    int line_h_a = (g_state.font_size == 2) ? 28 : (g_state.font_size == 1) ? 24 : 18;
    int16_t a_h = line_h_a + 8;

    if (g_state.scroll_mode != 1) {
      GSize a_size = graphics_text_layout_get_content_size(artist_text, artist_font, GRect(0, 0, max_width, 1000), overflow, GTextAlignmentCenter);
      a_h = a_size.h + 8;
    }

    int16_t max_h = zone_y - status_bottom;
    if (a_h > max_h) a_h = max_h;
    int16_t a_y = status_bottom + ((max_h - a_h) / 2);

    layer_set_frame(text_layer_get_layer(s_artist_layer), GRect(offset_x, a_y, max_width, a_h));
  }
}

// --- EXPORTS ---

void ui_apply_layout(void) {
  if (!g_state.window_loaded || s_marquee_spawn_anim) return;
  Layer *root = window_get_root_layer(g_state.window);
  GRect bounds = layer_get_bounds(root);

  int16_t max_width = PBL_IF_ROUND_ELSE(bounds.size.w - 24, bounds.size.w);
  int16_t offset_x = (bounds.size.w - max_width) / 2;

  if (!g_state.enable_watchface) {
    layer_set_frame(bitmap_layer_get_layer(s_logo_layer), GRect(0, NATIVE_Y(5, 12, bounds.size.h), bounds.size.w, NATIVE_H(35, 35, bounds.size.h)));
    layer_set_frame(s_status_layer, GRect(0, NATIVE_Y(68, 72, bounds.size.h), bounds.size.w, NATIVE_H(32, 32, bounds.size.h)));
    layer_set_frame(s_zone_layer, GRect(0, bounds.size.h - NATIVE_H(26, 32, bounds.size.h), bounds.size.w, NATIVE_H(26, 26, bounds.size.h)));

    int16_t track_y = NATIVE_Y(24, 28, bounds.size.h);
    int16_t track_h = NATIVE_H(52, 52, bounds.size.h);
    int16_t artist_y = NATIVE_Y(102, 106, bounds.size.h);
    int16_t artist_h = (g_state.scroll_mode == 2) ? (bounds.size.h - artist_y) : NATIVE_H(44, 44, bounds.size.h);

    GTextOverflowMode overflow_mode = (g_state.scroll_mode == 2) ? GTextOverflowModeWordWrap : GTextOverflowModeTrailingEllipsis;

    if (g_state.scroll_mode == 2) {
      GFont track_font = g_state.font_size == 2 ? fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD) :
      g_state.font_size == 1 ? fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD) :
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
      const char *track_text = text_layer_get_text(s_track_layer);
      if (track_text && strlen(track_text) > 0) {
        GSize t_size = graphics_text_layout_get_content_size(track_text, track_font, GRect(0, 0, max_width, 1000), overflow_mode, GTextAlignmentCenter);
        if (t_size.h + 8 > track_h) {
          int16_t diff = (t_size.h + 8) - track_h;
          track_h = t_size.h + 8;
          artist_y += diff;
        }
      }
    } else {
      track_h += 8;
      artist_h += 8;
    }

    layer_set_frame(text_layer_get_layer(s_track_layer), GRect(offset_x, track_y, max_width, track_h));
    layer_set_frame(text_layer_get_layer(s_artist_layer), GRect(offset_x, artist_y, max_width, artist_h));

    text_layer_set_overflow_mode(s_track_layer, overflow_mode);
    text_layer_set_overflow_mode(s_artist_layer, overflow_mode);

  } else if (!g_state.gesture_mode_active) {
    layer_set_frame(bitmap_layer_get_layer(s_logo_layer), GRect(0, NATIVE_Y(4, 10, bounds.size.h), bounds.size.w, NATIVE_H(20, 20, bounds.size.h)));
    layer_set_frame(s_status_layer, GRect(0, (bounds.size.h / 2) - 16, bounds.size.w, 32));

    int16_t status_bottom = (bounds.size.h / 2) + 16;
    int16_t start_y = status_bottom + 4;

    int16_t budget_h = bounds.size.h - start_y;
    int16_t track_y = start_y;
    int16_t track_h = 32;
    int16_t artist_y = start_y + track_h;
    int16_t artist_h = 28;

    GTextOverflowMode overflow_mode = (g_state.scroll_mode == 2) ? GTextOverflowModeWordWrap : GTextOverflowModeTrailingEllipsis;

    if (g_state.scroll_mode == 2) {
      int line_h = (g_state.font_size == 2) ? 28 : (g_state.font_size == 1) ? 24 : 18;
      int max_track_h = (line_h * 2) + 8;

      GFont track_font = g_state.font_size == 2 ? fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD) :
      g_state.font_size == 1 ? fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD) :
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

      const char *track_text = text_layer_get_text(s_track_layer);
      if (track_text && strlen(track_text) > 0) {
        GSize t_size = graphics_text_layout_get_content_size(track_text, track_font, GRect(0, 0, max_width, max_track_h), overflow_mode, GTextAlignmentCenter);
        track_h = t_size.h + 8;
        if (track_h > max_track_h) track_h = max_track_h;
        if (track_h > budget_h) track_h = budget_h;
      }

      artist_y = track_y + track_h;
      artist_h = budget_h - track_h;
      if (artist_h < 0) artist_h = 0;
    }

    layer_set_frame(text_layer_get_layer(s_track_layer), GRect(offset_x, track_y, max_width, track_h));
    layer_set_frame(text_layer_get_layer(s_artist_layer), GRect(offset_x, artist_y, max_width, artist_h));

    text_layer_set_overflow_mode(s_track_layer, overflow_mode);
    text_layer_set_overflow_mode(s_artist_layer, overflow_mode);
  } else {
    layer_set_frame(bitmap_layer_get_layer(s_logo_layer), GRect(0, NATIVE_Y(5, 12, bounds.size.h), bounds.size.w, NATIVE_H(35, 35, bounds.size.h)));
    layer_set_frame(s_status_layer, GRect(0, (bounds.size.h / 2) - 16, bounds.size.w, 32));
    layer_set_frame(s_zone_layer, GRect(0, bounds.size.h - NATIVE_H(26, 32, bounds.size.h), bounds.size.w, NATIVE_H(26, 26, bounds.size.h)));

    GTextOverflowMode overflow_mode = GTextOverflowModeTrailingEllipsis;
    text_layer_set_overflow_mode(s_track_layer, overflow_mode);
    text_layer_set_overflow_mode(s_artist_layer, overflow_mode);

    recalculate_gesture_centering(bounds);
  }
}

void ui_apply_theme(void) {
  if (!g_state.window_loaded) return;

  GColor bg_color = g_state.theme == 1 ? GColorWhite : GColorBlack;
  GColor fg_color = g_state.theme == 1 ? GColorBlack : GColorWhite;

  window_set_background_color(g_state.window, bg_color);
  text_layer_set_text_color(s_track_layer, fg_color);
  text_layer_set_text_color(s_artist_layer, fg_color);

  if (s_back_layer) text_layer_set_text_color(s_back_layer, fg_color);

  #if ENABLE_VOLUME
  if (s_vol_layer) {
    text_layer_set_background_color(s_vol_layer, bg_color);
    text_layer_set_text_color(s_vol_layer, fg_color);
  }
  #endif

  watchface_set_theme(g_state.theme == 1);

  if (s_logo_bitmap) gbitmap_destroy(s_logo_bitmap);
  s_logo_bitmap = gbitmap_create_with_resource(g_state.theme == 1 ? RESOURCE_ID_IMAGE_LOGO_LIGHT : RESOURCE_ID_IMAGE_LOGO_DARK);

  if (s_logo_layer) {
    bitmap_layer_set_bitmap(s_logo_layer, s_logo_bitmap);
    bitmap_layer_set_compositing_mode(s_logo_layer, GCompOpSet);
  }

  if (s_status_layer) layer_mark_dirty(s_status_layer);
  if (s_zone_layer) layer_mark_dirty(s_zone_layer);

  ui_update();
}

void ui_apply_fonts() {
  if (!s_track_layer || !s_artist_layer || !s_zone_layer) return;

  if (s_play_path) {
    gpath_destroy(s_play_path);
    s_play_path = NULL;
  }

  if (g_state.font_size == 2) {
    text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
    s_zone_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    s_play_path = gpath_create(&s_large_play_info);
  } else if (g_state.font_size == 1) {
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

  GTextOverflowMode overflow_mode = (g_state.scroll_mode == 2 && !(g_state.enable_watchface && g_state.gesture_mode_active)) ? GTextOverflowModeWordWrap : GTextOverflowModeTrailingEllipsis;

  text_layer_set_overflow_mode(s_track_layer, overflow_mode);
  text_layer_set_overflow_mode(s_artist_layer, overflow_mode);

  if (s_zone_layer) layer_mark_dirty(s_zone_layer);
}

bool ui_is_marquee_running(void) {
  return s_marquee_spawn_anim != NULL;
}

void ui_stop_marquee() {
  if (s_marquee_spawn_anim) {
    animation_unschedule(s_marquee_spawn_anim);
    s_marquee_spawn_anim = NULL;
  }

  if (s_track_layer && g_state.window_loaded) {
    ui_apply_layout();
    text_layer_set_text_alignment(s_track_layer, GTextAlignmentCenter);
    text_layer_set_text_alignment(s_artist_layer, GTextAlignmentCenter);
  }
}

void ui_start_marquee() {
  if (s_marquee_spawn_anim) return;
  ui_stop_marquee();

  if (!g_state.window_loaded || !s_track_layer || !s_artist_layer) return;
  if (g_state.enable_watchface && !g_state.gesture_mode_active) return;
  if (!g_state.app_in_focus) return;

  bool is_no_core = (strcmp(g_state.track_buf, "No Core") == 0);

  if (!is_no_core && g_state.scroll_mode != 1) return;

  const char* track_text = text_layer_get_text(s_track_layer);
  const char* artist_text = text_layer_get_text(s_artist_layer);

  if (!track_text || strlen(track_text) == 0) return;

  GFont font_track;
  GFont font_artist;
  if (is_no_core) {
    font_track = g_state.font_size == 2 ? fonts_get_system_font(FONT_KEY_GOTHIC_28) : (g_state.font_size == 1 ? fonts_get_system_font(FONT_KEY_GOTHIC_24) : fonts_get_system_font(FONT_KEY_GOTHIC_18));
    font_artist = font_track;
  } else {
    if (g_state.font_size == 2) {
      font_track = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
      font_artist = fonts_get_system_font(FONT_KEY_GOTHIC_28);
    } else if (g_state.font_size == 1) {
      font_track = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
      font_artist = fonts_get_system_font(FONT_KEY_GOTHIC_24);
    } else {
      font_track = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
      font_artist = fonts_get_system_font(FONT_KEY_GOTHIC_18);
    }
  }

  Layer *root = window_get_root_layer(g_state.window);
  GRect bounds = layer_get_bounds(root);

  GSize track_size = graphics_text_layout_get_content_size(track_text, font_track, GRect(0, 0, 2000, 60), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  GSize artist_size = graphics_text_layout_get_content_size(artist_text, font_artist, GRect(0, 0, 2000, 60), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

  bool scroll_track = (track_size.w > bounds.size.w);
  bool scroll_artist = (artist_size.w > bounds.size.w) || is_no_core;

  if (!scroll_track && !scroll_artist) return;

  int max_w = track_size.w > artist_size.w ? track_size.w : artist_size.w;
  int layer_width = max_w + 20;
  if (layer_width < bounds.size.w) layer_width = bounds.size.w;

  GRect t_frame = layer_get_frame(text_layer_get_layer(s_track_layer));
  GRect a_frame = layer_get_frame(text_layer_get_layer(s_artist_layer));

  GRect start_track = GRect(0, t_frame.origin.y, layer_width, t_frame.size.h);
  GRect end_track = GRect(-max_w - 20, t_frame.origin.y, layer_width, t_frame.size.h);

  GRect start_artist = GRect(0, a_frame.origin.y, layer_width, a_frame.size.h);
  GRect end_artist = GRect(-max_w - 20, a_frame.origin.y, layer_width, a_frame.size.h);

  int duration = (max_w + 20) * 20;
  if (is_no_core) duration = (max_w + 20) * 25;

  Animation *seq_t = NULL;
  Animation *seq_a = NULL;

  if (scroll_track && !is_no_core) {
    text_layer_set_text_alignment(s_track_layer, GTextAlignmentLeft);
    layer_set_frame(text_layer_get_layer(s_track_layer), start_track);

    PropertyAnimation *delay_prop_t = property_animation_create_layer_frame(text_layer_get_layer(s_track_layer), &start_track, &start_track);
    Animation *delay_anim_t = property_animation_get_animation(delay_prop_t);
    animation_set_duration(delay_anim_t, 1000);

    PropertyAnimation *scroll_prop_t = property_animation_create_layer_frame(text_layer_get_layer(s_track_layer), &start_track, &end_track);
    Animation *scroll_anim_t = property_animation_get_animation(scroll_prop_t);
    animation_set_curve(scroll_anim_t, AnimationCurveLinear);
    animation_set_duration(scroll_anim_t, duration);

    seq_t = animation_sequence_create(delay_anim_t, scroll_anim_t, NULL);
  }

  if (scroll_artist) {
    text_layer_set_text_alignment(s_artist_layer, GTextAlignmentLeft);
    layer_set_frame(text_layer_get_layer(s_artist_layer), start_artist);

    PropertyAnimation *delay_prop_a = property_animation_create_layer_frame(text_layer_get_layer(s_artist_layer), &start_artist, &start_artist);
    Animation *delay_anim_a = property_animation_get_animation(delay_prop_a);
    animation_set_duration(delay_anim_a, 1000);

    PropertyAnimation *scroll_prop_a = property_animation_create_layer_frame(text_layer_get_layer(s_artist_layer), &start_artist, &end_artist);
    Animation *scroll_anim_a = property_animation_get_animation(scroll_prop_a);
    animation_set_curve(scroll_anim_a, AnimationCurveLinear);
    animation_set_duration(scroll_anim_a, duration);

    seq_a = animation_sequence_create(delay_anim_a, scroll_anim_a, NULL);
  }

  if (seq_t && seq_a) {
    s_marquee_spawn_anim = animation_spawn_create(seq_t, seq_a, NULL);
  } else if (seq_t) {
    s_marquee_spawn_anim = seq_t;
  } else if (seq_a) {
    s_marquee_spawn_anim = seq_a;
  }

  if (s_marquee_spawn_anim) {
    animation_set_play_count(s_marquee_spawn_anim, ANIMATION_PLAY_COUNT_INFINITE);
    if (!animation_schedule(s_marquee_spawn_anim)) {
      animation_destroy(s_marquee_spawn_anim);
      s_marquee_spawn_anim = NULL;
    }
  }
}

void ui_show_temporary_message(const char *track, const char *artist) {
  if (s_track_layer && track) text_layer_set_text(s_track_layer, track);
  if (s_artist_layer && artist) text_layer_set_text(s_artist_layer, artist);
}

void ui_update() {
  if (!g_state.window_loaded) return;
  Layer *root = window_get_root_layer(g_state.window);
  GRect bounds = layer_get_bounds(root);

  bool is_error_resting = (g_state.mode == MODE_ERROR && g_state.enable_watchface && !g_state.gesture_mode_active);
  bool hide_music = false;
  bool quiet_hide = false;

  safe_set_text(s_track_layer, g_state.track_buf);

  if (strcmp(g_state.track_buf, "No Core") == 0) {
    safe_set_text(s_artist_layer, "Is the extension enabled?");
  } else {
    safe_set_text(s_artist_layer, g_state.artist_buf);
  }

  if (g_state.enable_watchface) {
    bool no_music = (g_state.track_buf[0] == '\0' || strcmp(g_state.track_buf, "Waiting for Roon...") == 0 || strcmp(g_state.track_buf, "No Core") == 0);

    #if !defined(PBL_BW)
    quiet_hide = (g_state.respect_quiet_time && quiet_time_is_active());
    #endif

    hide_music = is_error_resting || (quiet_hide && no_music);

    if (hide_music || (!g_state.gesture_mode_active && quiet_hide)) {
      ui_stop_marquee();
      layer_set_hidden(text_layer_get_layer(s_track_layer), true);
      layer_set_hidden(text_layer_get_layer(s_artist_layer), true);
      layer_set_hidden(s_zone_layer, true);
      if (s_status_layer) layer_set_hidden(s_status_layer, true);

      if (s_logo_layer) layer_set_hidden(bitmap_layer_get_layer(s_logo_layer), false);

      watchface_show_centered();
      watchface_set_date_visibility(true);
    } else {
      layer_set_hidden(text_layer_get_layer(s_track_layer), false);
      layer_set_hidden(text_layer_get_layer(s_artist_layer), false);

      if (!g_state.gesture_mode_active) {
        layer_set_hidden(s_zone_layer, true);
      } else {
        layer_set_hidden(s_zone_layer, (g_state.mode == MODE_ERROR));
      }

      if (s_status_layer) {
        bool hide_status = (!g_state.gesture_mode_active && quiet_hide);
        layer_set_hidden(s_status_layer, hide_status);
        layer_mark_dirty(s_status_layer);
      }
      if (s_logo_layer) layer_set_hidden(bitmap_layer_get_layer(s_logo_layer), false);

      if (g_state.gesture_mode_active) {
        watchface_hide();
      } else {
        watchface_show(NATIVE_Y(24, 30, bounds.size.h));
        watchface_set_date_visibility(false);
      }
    }
  } else {
    watchface_hide();
    layer_set_hidden(text_layer_get_layer(s_track_layer), false);
    layer_set_hidden(text_layer_get_layer(s_artist_layer), false);
    layer_set_hidden(s_zone_layer, false);

    if (s_status_layer) {
      layer_set_hidden(s_status_layer, (g_state.mode == MODE_ERROR));
      layer_mark_dirty(s_status_layer);
    }
    if (s_logo_layer) layer_set_hidden(bitmap_layer_get_layer(s_logo_layer), false);
  }

  if (s_back_layer) {
    layer_set_hidden(text_layer_get_layer(s_back_layer), !(g_state.enable_watchface && g_state.gesture_mode_active));
  }

  if (g_state.mode == MODE_ERROR) {
    if (g_state.enable_watchface && is_error_resting) {
      // UI concealed
    } else {
      ui_stop_marquee();
      safe_set_text(s_track_layer, "Bridge Not Found");
      safe_set_text(s_artist_layer, "Press SELECT to retry");
      if (s_zone_layer) layer_mark_dirty(s_zone_layer);
      if (s_status_layer) layer_set_hidden(s_status_layer, true);
    }
    #if ENABLE_VOLUME
    if (s_vol_layer) layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
    #endif
    return;
  }

  if (s_zone_layer) layer_mark_dirty(s_zone_layer);

  if (!hide_music && (!g_state.enable_watchface || g_state.gesture_mode_active || !quiet_hide)) {
    if (g_state.enable_watchface && g_state.gesture_mode_active) recalculate_gesture_centering(bounds);
    if (!s_marquee_spawn_anim && g_state.scroll_mode == 1) ui_start_marquee();
  }

  #if ENABLE_VOLUME
  if (s_vol_layer) {
    if (g_state.is_flashing_vol) {
      if (g_state.is_fixed) snprintf(g_state.vol_buf, sizeof(g_state.vol_buf), "Fixed Vol");
      else if (g_state.volume == -1) snprintf(g_state.vol_buf, sizeof(g_state.vol_buf), "Vol: --");
      else snprintf(g_state.vol_buf, sizeof(g_state.vol_buf), "Vol: %d", g_state.volume);

      text_layer_set_text(s_vol_layer, g_state.vol_buf);
      layer_set_hidden(text_layer_get_layer(s_vol_layer), false);
    } else {
      layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
    }
  }
  #endif
}

void ui_tick_handler(struct tm *tick_time) {
  watchface_tick_handler(tick_time);
  ui_update();
}

#if ENABLE_VOLUME
static void vol_flash_cb(void *data) {
  g_state.is_flashing_vol = false;
  s_vol_flash_timer = NULL;
  ui_update();
}
#endif

void ui_flash_volume(int ms) {
  #if ENABLE_VOLUME
  g_state.is_flashing_vol = true;
  if (s_vol_flash_timer) app_timer_cancel(s_vol_flash_timer);
  s_vol_flash_timer = app_timer_register(ms, vol_flash_cb, NULL);
  ui_update();
  #endif
}

void ui_cancel_vol_flash(void) {
  #if ENABLE_VOLUME
  g_state.is_flashing_vol = false;
  if (s_vol_flash_timer) {
    app_timer_cancel(s_vol_flash_timer);
    s_vol_flash_timer = NULL;
  }
  #endif
}

static void zone_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!g_state.window_loaded || (g_state.mode == MODE_ERROR && g_state.enable_watchface && !g_state.gesture_mode_active)) return;

  GRect bounds = layer_get_bounds(layer);
  const char* text_to_draw = (g_state.mode == MODE_ERROR) ? "Connection Error" : g_state.zone_buf;
  if (!s_zone_font || strlen(text_to_draw) == 0) return;

  GSize text_size = graphics_text_layout_get_content_size(text_to_draw, s_zone_font, GRect(0, 0, bounds.size.w - 12, bounds.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
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

  GColor bg_color = g_state.theme == 1 ? GColorWhite : GColorBlack;
  GColor fg_color = g_state.theme == 1 ? GColorBlack : GColorWhite;

  if (g_state.mode == MODE_ZONE) {
    graphics_context_set_fill_color(ctx, fg_color);
    graphics_fill_rect(ctx, box_rect, 3, GCornersAll);
    graphics_context_set_text_color(ctx, bg_color);
  } else {
    graphics_context_set_stroke_color(ctx, fg_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_round_rect(ctx, box_rect, 3);
    graphics_context_set_text_color(ctx, fg_color);
  }

  graphics_draw_text(ctx, text_to_draw, s_zone_font, text_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void status_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!g_state.window_loaded || g_state.mode == MODE_ERROR) return;

  GRect bounds = layer_get_bounds(layer);
  GColor main_color = g_state.theme == 1 ? GColorBlack : GColorWhite;

  if (g_state.mode == MODE_TRACK) {
    graphics_context_set_fill_color(ctx, main_color);
    if (g_state.is_playing) {
      if (g_state.font_size == 2) {
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 - 8, bounds.size.h/2 - 12, 6, 24), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 + 2, bounds.size.h/2 - 12, 6, 24), 0, GCornerNone);
      } else if (g_state.font_size == 1) {
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
    const char* mode_text = (g_state.mode == MODE_ZONE) ? "Select Zone" : "";
    graphics_draw_text(ctx, mode_text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(0, (bounds.size.h / 2) - 10, bounds.size.w, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

void ui_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, g_state.theme == 1 ? GColorWhite : GColorBlack);

  s_custom_font_42 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_COMFORTAA_BOLD_42));
  watchface_init(root, g_state.theme == 1, s_custom_font_42);

  s_logo_layer = bitmap_layer_create(GRect(0, NATIVE_Y(4, 10, bounds.size.h), bounds.size.w, NATIVE_H(20, 20, bounds.size.h)));
  bitmap_layer_set_background_color(s_logo_layer, GColorClear);
  bitmap_layer_set_alignment(s_logo_layer, GAlignCenter);
  layer_add_child(root, bitmap_layer_get_layer(s_logo_layer));

  GColor text_color = g_state.theme == 1 ? GColorBlack : GColorWhite;

  s_track_layer = text_layer_create(GRect(0, (bounds.size.h / 2) + NATIVE_Y(12, 14, bounds.size.h), bounds.size.w, NATIVE_H(32, 32, bounds.size.h)));
  text_layer_set_text(s_track_layer, "Loading...");
  text_layer_set_text_alignment(s_track_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_track_layer, GColorClear);
  text_layer_set_text_color(s_track_layer, text_color);
  layer_add_child(root, text_layer_get_layer(s_track_layer));

  s_status_layer = layer_create(GRect(0, (bounds.size.h / 2) - 16, bounds.size.w, 32));
  layer_set_update_proc(s_status_layer, status_layer_update_proc);
  layer_add_child(root, s_status_layer);

  s_artist_layer = text_layer_create(GRect(0, (bounds.size.h / 2) + NATIVE_Y(36, 40, bounds.size.h), bounds.size.w, NATIVE_H(28, 28, bounds.size.h)));
  text_layer_set_text_alignment(s_artist_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_artist_layer, GColorClear);
  text_layer_set_text_color(s_artist_layer, text_color);
  layer_add_child(root, text_layer_get_layer(s_artist_layer));

  s_zone_layer = layer_create(GRect(0, bounds.size.h - NATIVE_H(26, 32, bounds.size.h), bounds.size.w, NATIVE_H(26, 26, bounds.size.h)));
  layer_set_update_proc(s_zone_layer, zone_layer_update_proc);
  layer_add_child(root, s_zone_layer);

  // Gesture mode manual exit indicator
  s_back_layer = text_layer_create(GRect(0, NATIVE_Y(0, 20, bounds.size.h), 40, 40));
  text_layer_set_text(s_back_layer, "<");
  text_layer_set_font(s_back_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_back_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_back_layer, GColorClear);
  text_layer_set_text_color(s_back_layer, text_color);
  layer_set_hidden(text_layer_get_layer(s_back_layer), true);
  layer_add_child(root, text_layer_get_layer(s_back_layer));

  #if ENABLE_VOLUME
  s_vol_layer = text_layer_create(GRect(0, NATIVE_Y(60, 64, bounds.size.h), bounds.size.w, NATIVE_H(48, 48, bounds.size.h)));
  text_layer_set_text(s_vol_layer, "Vol: --");
  text_layer_set_font(s_vol_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_vol_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_vol_layer, g_state.theme == 1 ? GColorWhite : GColorBlack);
  text_layer_set_text_color(s_vol_layer, text_color);
  layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
  layer_add_child(root, text_layer_get_layer(s_vol_layer));
  #endif

  ui_apply_fonts();
  g_state.window_loaded = true;

  ui_apply_theme();
  ui_apply_layout();
  app_mark_user_interaction();

  if (g_state.scroll_mode == 1) ui_start_marquee();

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  watchface_tick_handler(t);
}

void ui_window_unload(Window *window) {
  g_state.window_loaded = false;

  watchface_deinit();

  if (s_play_path) { gpath_destroy(s_play_path); s_play_path = NULL; }

  ui_stop_marquee();

  #if ENABLE_VOLUME
  if (s_vol_flash_timer) app_timer_cancel(s_vol_flash_timer);
  text_layer_destroy(s_vol_layer);
  s_vol_layer = NULL;
  #endif

  text_layer_destroy(s_track_layer);
  text_layer_destroy(s_artist_layer);
  text_layer_destroy(s_back_layer);
  layer_destroy(s_zone_layer);
  layer_destroy(s_status_layer);
  bitmap_layer_destroy(s_logo_layer);
  gbitmap_destroy(s_logo_bitmap);

  fonts_unload_custom_font(s_custom_font_42);
}
