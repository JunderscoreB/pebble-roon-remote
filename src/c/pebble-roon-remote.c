#include <pebble.h>

#define KEY_COMMAND 0
#define KEY_ZONE_NAME 1
#define KEY_TRACK 2
#define KEY_ARTIST 3
#define KEY_IS_PLAYING 4
#define KEY_VOLUME_VAL 5
#define KEY_IS_FIXED 6

// VOLUME FEATURE DISABLED
#define ENABLE_VOLUME 0

typedef enum {
  MODE_TRACK,
  MODE_ZONE
  #if ENABLE_VOLUME
  ,MODE_VOLUME
  #endif
} AppMode;

static AppMode s_mode = MODE_TRACK;
static Window *s_window;
static bool s_window_loaded = false;

// UI Layers
static BitmapLayer *s_logo_layer = NULL;
static GBitmap *s_logo_bitmap = NULL;
static TextLayer *s_track_layer = NULL;
static TextLayer *s_artist_layer = NULL;
static TextLayer *s_zone_layer = NULL;
static Layer *s_status_layer = NULL; 

#if ENABLE_VOLUME
static TextLayer *s_vol_layer = NULL;
static AppTimer *s_vol_revert_timer = NULL;
static char s_vol_buf[32];
static int s_volume = -1; 
#endif

// Timers
static AppTimer *s_network_cooldown_timer = NULL;
static AppTimer *s_playpause_delay_timer = NULL;
static AppTimer *s_zone_revert_timer = NULL; 

// Buffers
static char s_zone_buf[64];

// Data
static bool s_is_playing = false; 
static bool s_is_fixed = false;
static bool s_network_ready = true;

// --- FORWARD DECLARATIONS ---
static void update_ui();

// --- SAFETY HELPER ---
static int get_tuple_int(Tuple *t) {
  if (!t) return -1;
  switch (t->length) {
    case 1: return t->value->int8;
    case 2: return t->value->int16;
    case 4: return t->value->int32;
    default: return t->value->int32;
  }
}

// --- NETWORK THROTTLE ---
static void cooldown_cb(void *data) {
  s_network_ready = true;
  s_network_cooldown_timer = NULL;
}

static void trigger_cooldown() {
  s_network_ready = false;
  if (s_network_cooldown_timer) app_timer_cancel(s_network_cooldown_timer);
  s_network_cooldown_timer = app_timer_register(250, cooldown_cb, NULL);
}

// --- NETWORK ---
static void send_command(char *cmd) {
  if (!s_window_loaded) return;
  if (!s_network_ready) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "TX Throttled: %s", cmd);
    return;
  }

  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_cstring(iter, KEY_COMMAND, cmd);
    app_message_outbox_send();
    APP_LOG(APP_LOG_LEVEL_INFO, "TX: %s", cmd);
    trigger_cooldown();
  }
}

// --- UI HELPERS ---
static void safe_set_text(TextLayer *layer, char *text) {
  if (s_window_loaded && layer && text) {
    text_layer_set_text(layer, text);
  }
}

static void update_ui() {
  if (!s_window_loaded) return;

  // ZONE DISPLAY
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
  // VOLUME DISPLAY
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

// --- ZONE TIMER ---
static void zone_revert_callback(void *data) {
  s_zone_revert_timer = NULL;
  if (s_mode == MODE_ZONE) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Zone Timeout: Reverting to TRACK");
    s_mode = MODE_TRACK;
    update_ui();
  }
}

static void reset_zone_timer() {
  if (s_zone_revert_timer) app_timer_cancel(s_zone_revert_timer);
  s_zone_revert_timer = app_timer_register(4000, zone_revert_callback, NULL);
}

static void cancel_zone_timer() {
  if (s_zone_revert_timer) {
    app_timer_cancel(s_zone_revert_timer);
    s_zone_revert_timer = NULL;
  }
}

#if ENABLE_VOLUME
// --- VOLUME TIMER ---
static void vol_revert_callback(void *data) {
  s_vol_revert_timer = NULL;
  if (s_mode == MODE_VOLUME) {
    s_mode = MODE_TRACK;
    update_ui();
  }
}

static void reset_vol_timer() {
  if (s_vol_revert_timer) app_timer_cancel(s_vol_revert_timer);
  s_vol_revert_timer = app_timer_register(4000, vol_revert_callback, NULL);
}

static void cancel_vol_timer() {
  if (s_vol_revert_timer) {
    app_timer_cancel(s_vol_revert_timer);
    s_vol_revert_timer = NULL;
  }
}
#endif

// --- BUTTONS ---
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_mode == MODE_TRACK) {
    send_command("previous");
  } 
  else if (s_mode == MODE_ZONE) {
    reset_zone_timer(); 
    send_command("prev_zone");
  }
  #if ENABLE_VOLUME
  else if (s_mode == MODE_VOLUME) {
    reset_vol_timer();
    send_command("vol_up");
  }
  #endif
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_mode == MODE_TRACK) {
    send_command("next");
  } 
  else if (s_mode == MODE_ZONE) {
    reset_zone_timer(); 
    send_command("next_zone");
  }
  #if ENABLE_VOLUME
  else if (s_mode == MODE_VOLUME) {
    reset_vol_timer();
    send_command("vol_down");
  }
  #endif
}

// SELECT (Short)
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_mode == MODE_TRACK) {
    s_mode = MODE_ZONE;
    reset_zone_timer(); 
  }
  else if (s_mode == MODE_ZONE) {
    cancel_zone_timer();
    send_command("status");
    s_mode = MODE_TRACK;
  }
  #if ENABLE_VOLUME
  else if (s_mode == MODE_VOLUME) {
    cancel_vol_timer();
    s_mode = MODE_ZONE;
  }
  #endif
  
  update_ui();
}

static void send_playpause_cb(void *data) {
  s_playpause_delay_timer = NULL;
  send_command("playpause");
}

// SELECT (Long) - Global Play/Pause
static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  vibes_short_pulse();
  if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
  s_playpause_delay_timer = app_timer_register(100, send_playpause_cb, NULL);
  
  if (s_mode == MODE_ZONE) reset_zone_timer();
  #if ENABLE_VOLUME
  if (s_mode == MODE_VOLUME) reset_vol_timer();
  #endif
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 800, select_long_click_handler, NULL);
}

// --- APP SETUP ---

static void status_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_window_loaded) return;
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  if (s_is_playing) {
    graphics_fill_rect(ctx, GRect(bounds.size.w/2 - 6, bounds.size.h/2 - 8, 4, 16), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(bounds.size.w/2 + 2, bounds.size.h/2 - 8, 4, 16), 0, GCornerNone);
  } else {
    GPoint p1 = GPoint(bounds.size.w/2 - 4, bounds.size.h/2 - 8);
    GPoint p2 = GPoint(bounds.size.w/2 - 4, bounds.size.h/2 + 8);
    GPoint p3 = GPoint(bounds.size.w/2 + 8, bounds.size.h/2);
    GPathInfo triangle_info = { .num_points = 3, .points = (GPoint []) {p1, p2, p3} };
    GPath *triangle_path = gpath_create(&triangle_info);
    gpath_draw_filled(ctx, triangle_path);
    gpath_destroy(triangle_path);
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  if (!s_window_loaded) return;
  Tuple *t;
  if ((t = dict_find(iterator, KEY_ZONE_NAME))) {
    snprintf(s_zone_buf, sizeof(s_zone_buf), "%s", t->value->cstring);
    if (s_zone_layer) safe_set_text(s_zone_layer, s_zone_buf);
  }
  if ((t = dict_find(iterator, KEY_TRACK))) safe_set_text(s_track_layer, t->value->cstring);
  if ((t = dict_find(iterator, KEY_ARTIST))) safe_set_text(s_artist_layer, t->value->cstring);
  if ((t = dict_find(iterator, KEY_IS_PLAYING))) {
    s_is_playing = (t->value->int32 == 1);
    if (s_status_layer) layer_mark_dirty(s_status_layer);
  }
  
  #if ENABLE_VOLUME
  if ((t = dict_find(iterator, KEY_VOLUME_VAL))) {
    s_volume = get_tuple_int(t);
    if (s_mode == MODE_VOLUME) update_ui();
  }
  #endif
  
  if ((t = dict_find(iterator, KEY_IS_FIXED))) s_is_fixed = (t->value->int32 == 1);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorBlack);

  s_logo_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_LOGO);
  s_logo_layer = bitmap_layer_create(GRect(0, 5, bounds.size.w, 40));
  bitmap_layer_set_background_color(s_logo_layer, GColorClear);
  bitmap_layer_set_bitmap(s_logo_layer, s_logo_bitmap);
  bitmap_layer_set_compositing_mode(s_logo_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_logo_layer, GAlignCenter);
  layer_add_child(root, bitmap_layer_get_layer(s_logo_layer));

  // REDUCED FONT SIZES
  s_track_layer = text_layer_create(GRect(0, 45, bounds.size.w, 55));
  text_layer_set_text(s_track_layer, "Loading...");
  text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_track_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_track_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_track_layer, GColorClear);
  text_layer_set_text_color(s_track_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_track_layer));

  s_artist_layer = text_layer_create(GRect(0, 100, bounds.size.w, 25));
  text_layer_set_text_alignment(s_artist_layer, GTextAlignmentCenter);
  text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_background_color(s_artist_layer, GColorClear);
  text_layer_set_text_color(s_artist_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_artist_layer));
  
  s_status_layer = layer_create(GRect(0, 125, bounds.size.w, 20));
  layer_set_update_proc(s_status_layer, status_layer_update_proc);
  layer_add_child(root, s_status_layer);

  s_zone_layer = text_layer_create(GRect(0, 148, bounds.size.w, 25));
  text_layer_set_text(s_zone_layer, "Connecting...");
  text_layer_set_font(s_zone_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_zone_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_zone_layer, GColorClear);
  text_layer_set_text_color(s_zone_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_zone_layer));
  
  #if ENABLE_VOLUME
  s_vol_layer = text_layer_create(GRect(0, 45, bounds.size.w, 80));
  text_layer_set_text(s_vol_layer, "Vol: --");
  text_layer_set_font(s_vol_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD)); 
  text_layer_set_text_alignment(s_vol_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_vol_layer, GColorBlack);
  text_layer_set_text_color(s_vol_layer, GColorWhite);
  layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
  layer_add_child(root, text_layer_get_layer(s_vol_layer));
  #endif
  
  s_window_loaded = true;
  APP_LOG(APP_LOG_LEVEL_INFO, "Window Load Complete");
}

static void window_unload(Window *window) {
  s_window_loaded = false; 
  if(s_network_cooldown_timer) app_timer_cancel(s_network_cooldown_timer);
  if(s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
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
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) { .load = window_load, .unload = window_unload });
  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(512, 512);
  window_stack_push(s_window, true);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
