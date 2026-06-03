#include <pebble.h>

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

#define PERSIST_KEY_FONT 0
#define PERSIST_KEY_SCROLL 1

#define ENABLE_VOLUME 1

#define RECT_SCALE_Y(val) (((val) * bounds.size.h) / 168)
#define RECT_SCALE_H(val) (((val) * bounds.size.h) / 168)
#define NATIVE_Y(rect_y, round_y) PBL_IF_ROUND_ELSE(round_y, RECT_SCALE_Y(rect_y))
#define NATIVE_H(rect_h, round_h) PBL_IF_ROUND_ELSE(round_h, RECT_SCALE_H(rect_h))

typedef enum {
  MODE_TRACK,
  MODE_ZONE,
  MODE_ERROR
  #if ENABLE_VOLUME
  ,MODE_VOLUME
  #endif
} AppMode;

static AppMode s_mode = MODE_TRACK;
static Window *s_window;
static bool s_window_loaded = false;

// Configs
static int s_font_size = 1; // 0=Small, 1=Normal, 2=Large
static bool s_enable_scroll = false; 

// UI Layers
static BitmapLayer *s_logo_layer = NULL;
static GBitmap *s_logo_bitmap = NULL;
static TextLayer *s_track_layer = NULL;
static TextLayer *s_artist_layer = NULL;
static TextLayer *s_zone_layer = NULL;
static Layer *s_status_layer = NULL;

// Render Objects
static GPath *s_play_path = NULL;
static const GPathInfo s_play_path_info = {
  .num_points = 3,
  .points = (GPoint []) {{-4, -8}, {-4, 8}, {8, 0}}
};
static PropertyAnimation *s_marquee_anim = NULL;

#if ENABLE_VOLUME
static TextLayer *s_vol_layer = NULL;
static AppTimer *s_vol_revert_timer = NULL;
static char s_vol_buf[32];
static int s_volume = -1;
#endif

// Timers & State
static AppTimer *s_network_cooldown_timer = NULL;
static AppTimer *s_playpause_delay_timer = NULL;
static AppTimer *s_zone_revert_timer = NULL;

static bool s_is_playing = false;
static bool s_is_fixed = false;
static bool s_network_ready = true;

// --- DEDICATED MEMORY BUFFERS ---
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

static void cooldown_cb(void *data) {
  s_network_ready = true;
  s_network_cooldown_timer = NULL;
}

static void trigger_cooldown() {
  s_network_ready = false;
  if (s_network_cooldown_timer) app_timer_cancel(s_network_cooldown_timer);
  s_network_cooldown_timer = app_timer_register(250, cooldown_cb, NULL);
}

static void send_command(char *cmd) {
  if (!s_window_loaded) return;
  if (!s_network_ready) return;

  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_cstring(iter, KEY_COMMAND, cmd);
    app_message_outbox_send();
    trigger_cooldown();
  }
}

static void safe_set_text(TextLayer *layer, char *text) {
  if (s_window_loaded && layer && text) text_layer_set_text(layer, text);
}

static void apply_fonts() {
  if (!s_track_layer || !s_artist_layer || !s_zone_layer) return;
  
  if (s_font_size == 2) { // LARGE
    text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28)); 
    text_layer_set_font(s_zone_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  } else if (s_font_size == 1) { // NORMAL
    text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24)); 
    text_layer_set_font(s_zone_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  } else { // SMALL
    text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18)); 
    text_layer_set_font(s_zone_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  }
}

// --- MARQUEE ENGINE ---
static void stop_marquee() {
  if (s_marquee_anim) {
    animation_unschedule(property_animation_get_animation(s_marquee_anim));
    property_animation_destroy(s_marquee_anim);
    s_marquee_anim = NULL;
  }
  if (s_track_layer && s_window_loaded) {
    Layer *root = window_get_root_layer(s_window);
    GRect bounds = layer_get_bounds(root);
    layer_set_frame(text_layer_get_layer(s_track_layer), GRect(0, NATIVE_Y(36, 44), bounds.size.w, NATIVE_H(56, 56)));
    text_layer_set_text_alignment(s_track_layer, GTextAlignmentCenter);
  }
}

static void start_marquee() {
  stop_marquee();
  if (!s_enable_scroll || !s_window_loaded || !s_track_layer) return;

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

    // Slide entirely across the screen
    GRect start = GRect(bounds.size.w, NATIVE_Y(36, 44), text_size.w + 20, NATIVE_H(56, 56));
    GRect end = GRect(-text_size.w - 20, NATIVE_Y(36, 44), text_size.w + 20, NATIVE_H(56, 56));

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
    safe_set_text(s_zone_layer, "Connection Error");

    if (s_status_layer) layer_set_hidden(s_status_layer, true);

    #if ENABLE_VOLUME
    if (s_vol_layer) layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
    #endif
    return;
  }

  if (s_status_layer) layer_set_hidden(s_status_layer, false);

  if (s_zone_layer) {
    safe_set_text(s_zone_layer, s_zone_buf);
    if (s_mode == MODE_ZONE) {
      text_layer_set_background_color(s_zone_layer, GColorWhite);
      text_layer_set_text_color(s_zone_layer, GColorBlack);
    } else {
      text_layer_set_background_color(s_zone_layer, GColorClear);
      text_layer_set_text_color(s_zone_layer, GColorWhite);
    }
  }

  #if ENABLE_VOLUME
  if (s_vol_layer) {
    if (s_mode == MODE_VOLUME) {
      if (s_is_fixed) snprintf(s_vol_buf, sizeof(s_vol_buf), "Fixed");
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

static void zone_revert_callback(void *data) {
  s_zone_revert_timer = NULL;
  if (s_mode == MODE_ZONE) { s_mode = MODE_TRACK; update_ui(); }
}

static void reset_zone_timer() {
  if (s_zone_revert_timer) app_timer_cancel(s_zone_revert_timer);
  s_zone_revert_timer = app_timer_register(4000, zone_revert_callback, NULL);
}

static void cancel_zone_timer() {
  if (s_zone_revert_timer) { app_timer_cancel(s_zone_revert_timer); s_zone_revert_timer = NULL; }
}

#if ENABLE_VOLUME
static void vol_revert_callback(void *data) {
  s_vol_revert_timer = NULL;
  if (s_mode == MODE_VOLUME) { s_mode = MODE_TRACK; update_ui(); }
}

static void reset_vol_timer() {
  if (s_vol_revert_timer) app_timer_cancel(s_vol_revert_timer);
  s_vol_revert_timer = app_timer_register(4000, vol_revert_callback, NULL);
}

static void cancel_vol_timer() {
  if (s_vol_revert_timer) { app_timer_cancel(s_vol_revert_timer); s_vol_revert_timer = NULL; }
}
#endif

// --- BUTTON CONTROLS ---

// NEW: Overrides the default Back button behavior
static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_mode == MODE_ZONE) {
    cancel_zone_timer();
    s_mode = MODE_TRACK;
    update_ui();
  }
  #if ENABLE_VOLUME
  else if (s_mode == MODE_VOLUME) {
    cancel_vol_timer();
    s_mode = MODE_TRACK;
    update_ui();
  }
  #endif
  else {
    // If we are already in the main track mode, pop the window to close the app properly
    window_stack_pop(true);
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_mode == MODE_ERROR) return;
  if (s_mode == MODE_TRACK) send_command("previous");
  else if (s_mode == MODE_ZONE) { reset_zone_timer(); send_command("prev_zone"); }
  #if ENABLE_VOLUME
  else if (s_mode == MODE_VOLUME) { reset_vol_timer(); send_command("vol_up"); }
  #endif
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_mode == MODE_ERROR) return;
  if (s_mode == MODE_TRACK) send_command("next");
  else if (s_mode == MODE_ZONE) { reset_zone_timer(); send_command("next_zone"); }
  #if ENABLE_VOLUME
  else if (s_mode == MODE_VOLUME) { reset_vol_timer(); send_command("vol_down"); }
  #endif
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_mode == MODE_ERROR) {
    safe_set_text(s_track_layer, "Retrying...");
    safe_set_text(s_artist_layer, "Please wait...");
    send_command("retry_connection");
    return;
  }
  if (s_mode == MODE_TRACK) { s_mode = MODE_ZONE; reset_zone_timer(); }
  else if (s_mode == MODE_ZONE) { cancel_zone_timer(); send_command("status"); s_mode = MODE_TRACK; }
  #if ENABLE_VOLUME
  else if (s_mode == MODE_VOLUME) { cancel_vol_timer(); s_mode = MODE_ZONE; }
  #endif
  update_ui();
}

static void send_playpause_cb(void *data) {
  s_playpause_delay_timer = NULL;
  send_command("playpause");
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_mode == MODE_ERROR) return;
  vibes_short_pulse();
  if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
  s_playpause_delay_timer = app_timer_register(100, send_playpause_cb, NULL);
  if (s_mode == MODE_ZONE) reset_zone_timer();
  #if ENABLE_VOLUME
  if (s_mode == MODE_VOLUME) reset_vol_timer();
  #endif
}

static void click_config_provider(void *context) {
  // NEW: Subscribe the Back Button to our custom handler
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
  
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 800, select_long_click_handler, NULL);
}

#ifdef PBL_TOUCH
static void touch_handler(const TouchEvent *event, void *context) {
  if (s_mode == MODE_ERROR) return;
  if (event->type == TouchEvent_Touchdown) {
    Layer *window_layer = window_get_root_layer(s_window);
    GRect bounds = layer_get_bounds(window_layer);
    GRect status_target = GRect(0, bounds.size.h / 2, bounds.size.w, bounds.size.h / 2);
    GPoint tap_loc = GPoint(event->x, event->y);
    if (grect_contains_point(&status_target, &tap_loc)) {
      vibes_short_pulse(); 
      if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
      s_playpause_delay_timer = app_timer_register(100, send_playpause_cb, NULL);
      if (s_mode == MODE_ZONE) reset_zone_timer();
      #if ENABLE_VOLUME
      if (s_mode == MODE_VOLUME) reset_vol_timer();
      #endif
    }
  }
}
#endif

#ifndef PBL_TOUCH
static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_mode == MODE_ERROR) return;
  if (axis == ACCEL_AXIS_Z) {
    vibes_short_pulse(); 
    if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
    s_playpause_delay_timer = app_timer_register(100, send_playpause_cb, NULL);
    if (s_mode == MODE_ZONE) reset_zone_timer();
    #if ENABLE_VOLUME
    if (s_mode == MODE_VOLUME) reset_vol_timer();
    #endif
  }
}
#endif

static void status_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_window_loaded || s_mode == MODE_ERROR) return;
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  if (s_is_playing) {
    graphics_fill_rect(ctx, GRect(bounds.size.w/2 - 6, bounds.size.h/2 - 8, 4, 16), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(bounds.size.w/2 + 2, bounds.size.h/2 - 8, 4, 16), 0, GCornerNone);
  } else {
    if (s_play_path) {
      gpath_move_to(s_play_path, GPoint(bounds.size.w/2, bounds.size.h/2));
      gpath_draw_filled(ctx, s_play_path);
    }
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  if (!s_window_loaded) return;
  Tuple *t;

  if ((t = dict_find(iterator, KEY_ERROR))) {
    if (get_tuple_int(t) == 1) { s_mode = MODE_ERROR; update_ui(); return; } 
    else { if (s_mode == MODE_ERROR) s_mode = MODE_TRACK; }
  }
  if (s_mode == MODE_ERROR) return;

  if ((t = dict_find(iterator, KEY_FONT_SIZE))) {
    int requested_size = get_tuple_int(t);
    if (s_font_size != requested_size) {
      s_font_size = requested_size;
      persist_write_int(PERSIST_KEY_FONT, s_font_size);
      apply_fonts();
      start_marquee(); 
    }
  }

  if ((t = dict_find(iterator, KEY_SCROLL_TEXT))) {
    bool requested_scroll = (get_tuple_int(t) == 1);
    if (s_enable_scroll != requested_scroll) {
      s_enable_scroll = requested_scroll;
      persist_write_bool(PERSIST_KEY_SCROLL, s_enable_scroll);
      start_marquee(); 
    }
  }

  if ((t = dict_find(iterator, KEY_ZONE_NAME))) {
    if (strcmp(s_zone_buf, t->value->cstring) != 0) {
      snprintf(s_zone_buf, sizeof(s_zone_buf), "%s", t->value->cstring);
      if (s_zone_layer) safe_set_text(s_zone_layer, s_zone_buf);
    }
  }
  
  if ((t = dict_find(iterator, KEY_TRACK))) {
    if (strcmp(s_track_buf, t->value->cstring) != 0) {
      snprintf(s_track_buf, sizeof(s_track_buf), "%s", t->value->cstring);
      safe_set_text(s_track_layer, s_track_buf);
      start_marquee(); 
    }
  }
  
  if ((t = dict_find(iterator, KEY_ARTIST))) {
    if (strcmp(s_artist_buf, t->value->cstring) != 0) {
      snprintf(s_artist_buf, sizeof(s_artist_buf), "%s", t->value->cstring);
      safe_set_text(s_artist_layer, s_artist_buf);
    }
  }

  if ((t = dict_find(iterator, KEY_IS_PLAYING))) {
    s_is_playing = (get_tuple_int(t) == 1);
    if (s_status_layer) layer_mark_dirty(s_status_layer);
  }

  #if ENABLE_VOLUME
  if ((t = dict_find(iterator, KEY_VOLUME_VAL))) {
    s_volume = get_tuple_int(t);
    if (s_mode == MODE_VOLUME) update_ui();
  }
  #endif

  if ((t = dict_find(iterator, KEY_IS_FIXED))) s_is_fixed = (get_tuple_int(t) == 1);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorBlack);

  s_play_path = gpath_create(&s_play_path_info);

  s_logo_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_LOGO);
  s_logo_layer = bitmap_layer_create(GRect(0, NATIVE_Y(5, 12), bounds.size.w, NATIVE_H(35, 35)));
  bitmap_layer_set_background_color(s_logo_layer, GColorClear);
  bitmap_layer_set_bitmap(s_logo_layer, s_logo_bitmap);
  bitmap_layer_set_compositing_mode(s_logo_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_logo_layer, GAlignCenter);
  layer_add_child(root, bitmap_layer_get_layer(s_logo_layer));

  s_track_layer = text_layer_create(GRect(0, NATIVE_Y(36, 44), bounds.size.w, NATIVE_H(56, 56)));
  text_layer_set_text(s_track_layer, "Loading...");
  text_layer_set_text_alignment(s_track_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_track_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(s_track_layer, GColorClear);
  text_layer_set_text_color(s_track_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_track_layer));

  s_artist_layer = text_layer_create(GRect(0, NATIVE_Y(92, 100), bounds.size.w, NATIVE_H(42, 42)));
  text_layer_set_text_alignment(s_artist_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_artist_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(s_artist_layer, GColorClear);
  text_layer_set_text_color(s_artist_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_artist_layer));

  s_status_layer = layer_create(GRect(0, NATIVE_Y(134, 138), bounds.size.w, NATIVE_H(16, 16)));
  layer_set_update_proc(s_status_layer, status_layer_update_proc);
  layer_add_child(root, s_status_layer);

  s_zone_layer = text_layer_create(GRect(0, NATIVE_Y(150, 150), bounds.size.w, NATIVE_H(18, 24)));
  text_layer_set_text(s_zone_layer, "Connecting...");
  text_layer_set_text_alignment(s_zone_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_zone_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(s_zone_layer, GColorClear);
  text_layer_set_text_color(s_zone_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_zone_layer));

  #if ENABLE_VOLUME
  s_vol_layer = text_layer_create(GRect(0, NATIVE_Y(45, 50), bounds.size.w, NATIVE_H(80, 80)));
  text_layer_set_text(s_vol_layer, "Vol: --");
  text_layer_set_font(s_vol_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_vol_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_vol_layer, GColorBlack);
  text_layer_set_text_color(s_vol_layer, GColorWhite);
  layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
  layer_add_child(root, text_layer_get_layer(s_vol_layer));
  #endif

  apply_fonts();
  s_window_loaded = true;
}

static void window_unload(Window *window) {
  s_window_loaded = false;
  
  if (s_play_path) { gpath_destroy(s_play_path); s_play_path = NULL; }
  if (s_network_cooldown_timer) app_timer_cancel(s_network_cooldown_timer);
  if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
  
  stop_marquee();
  cancel_zone_timer();

  #if ENABLE_VOLUME
  cancel_vol_timer();
  text_layer_destroy(s_vol_layer);
  s_vol_layer = NULL;
  #endif

  text_layer_destroy(s_track_layer);
  text_layer_destroy(s_artist_layer);
  text_layer_destroy(s_zone_layer);
  layer_destroy(s_status_layer);
  bitmap_layer_destroy(s_logo_layer);
  gbitmap_destroy(s_logo_bitmap);

  s_track_layer = NULL;
  s_artist_layer = NULL;
  s_zone_layer = NULL;
  s_status_layer = NULL;
  s_logo_layer = NULL;
  s_logo_bitmap = NULL;
}

static void init(void) {
  if (persist_exists(PERSIST_KEY_FONT)) s_font_size = persist_read_int(PERSIST_KEY_FONT);
  if (persist_exists(PERSIST_KEY_SCROLL)) s_enable_scroll = persist_read_bool(PERSIST_KEY_SCROLL);

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  
  #ifdef PBL_TOUCH
  if (touch_service_is_enabled()) touch_service_subscribe(touch_handler, NULL);
  #else
  accel_tap_service_subscribe(accel_tap_handler);
  #endif
  
  window_set_window_handlers(s_window, (WindowHandlers) { .load = window_load, .unload = window_unload });
  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
  window_stack_push(s_window, true);
}

static void deinit(void) {
  #ifdef PBL_TOUCH
  touch_service_unsubscribe();
  #else
  accel_tap_service_unsubscribe();
  #endif
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}