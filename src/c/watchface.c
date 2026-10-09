/*
 * Pebble Roon Remote - Watchface Module
 * Copyright (c) 2026 J_B
 */

#include <pebble.h>
#include "watchface.h"

static TextLayer *s_time_layer = NULL;
static TextLayer *s_date_layer = NULL;
static char s_time_buf[16] = "";
static char s_date_buf[64] = "";
static bool s_light_mode = false;

#define RECT_SCALE_Y_LOCAL(val, h) (((val) * (h)) / 168)
#define RECT_SCALE_H_LOCAL(val, h) (((val) * (h)) / 168)
#define ROUND_SCALE_Y_LOCAL(val, h) (((val) * (h)) / 180)
#define ROUND_SCALE_H_LOCAL(val, h) (((val) * (h)) / 180)
#define NATIVE_Y_LOCAL(rect_y, round_y, h) PBL_IF_ROUND_ELSE(ROUND_SCALE_Y_LOCAL(round_y, h), RECT_SCALE_Y_LOCAL(rect_y, h))
#define NATIVE_H_LOCAL(rect_h, round_h, h) PBL_IF_ROUND_ELSE(ROUND_SCALE_H_LOCAL(round_h, h), RECT_SCALE_H_LOCAL(rect_h, h))

void watchface_init(Layer *root_layer, bool is_light_mode, GFont time_font) {
    s_light_mode = is_light_mode;
    GRect bounds = layer_get_bounds(root_layer);

    // Primary Clock Layer
    s_time_layer = text_layer_create(GRect(0, NATIVE_Y_LOCAL(15, 20, bounds.size.h), bounds.size.w, NATIVE_H_LOCAL(42, 42, bounds.size.h)));
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text_color(s_time_layer, s_light_mode ? GColorBlack : GColorWhite);
    text_layer_set_font(s_time_layer, time_font);
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
    layer_add_child(root_layer, text_layer_get_layer(s_time_layer));

    // Secondary Date Layer
    s_date_layer = text_layer_create(GRect(0, NATIVE_Y_LOCAL(58, 64, bounds.size.h), bounds.size.w, NATIVE_H_LOCAL(44, 44, bounds.size.h)));
    text_layer_set_background_color(s_date_layer, GColorClear);
    text_layer_set_text_color(s_date_layer, s_light_mode ? GColorBlack : GColorWhite);
    text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
    text_layer_set_overflow_mode(s_date_layer, GTextOverflowModeWordWrap);
    layer_set_hidden(text_layer_get_layer(s_date_layer), true);
    layer_add_child(root_layer, text_layer_get_layer(s_date_layer));
}

void watchface_deinit(void) {
    if (s_time_layer) { text_layer_destroy(s_time_layer); s_time_layer = NULL; }
    if (s_date_layer) { text_layer_destroy(s_date_layer); s_date_layer = NULL; }
}

void watchface_tick_handler(struct tm *tick_time) {
    if (clock_is_24h_style()) {
        strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", tick_time);
    } else {
        char temp_buf[8];
        strftime(temp_buf, sizeof(temp_buf), "%I:%M", tick_time);
        snprintf(s_time_buf, sizeof(s_time_buf), "%s %s", &temp_buf[temp_buf[0] == '0' ? 1 : 0], tick_time->tm_hour < 12 ? "am" : "pm");
    }
    if (s_time_layer) text_layer_set_text(s_time_layer, s_time_buf);

    static int s_last_yday = -1;
    if (s_last_yday != tick_time->tm_yday) {
        s_last_yday = tick_time->tm_yday;
        if (s_time_layer) {
            // Shortened date, omitting the year
            strftime(s_date_buf, sizeof(s_date_buf), "%a, %b %e", tick_time);
        }
        if (s_date_layer) text_layer_set_text(s_date_layer, s_date_buf);
    }
}

void watchface_set_theme(bool is_light_mode) {
    s_light_mode = is_light_mode;
    GColor text_color = s_light_mode ? GColorBlack : GColorWhite;
    if (s_time_layer) text_layer_set_text_color(s_time_layer, text_color);
    if (s_date_layer) text_layer_set_text_color(s_date_layer, text_color);
}

void watchface_show(int16_t vertical_offset) {
    if (s_time_layer) {
        Layer *root_layer = window_get_root_layer(layer_get_window(text_layer_get_layer(s_time_layer)));
        GRect bounds = layer_get_bounds(root_layer);

        int time_h = NATIVE_H_LOCAL(42, 42, bounds.size.h);
        int default_y = NATIVE_Y_LOCAL(15, 20, bounds.size.h);
        int final_y = (vertical_offset >= 0) ? vertical_offset : default_y;

        layer_set_frame(text_layer_get_layer(s_time_layer), GRect(0, final_y, bounds.size.w, time_h));
        layer_set_hidden(text_layer_get_layer(s_time_layer), false);

        if (s_date_layer) {
            layer_set_hidden(text_layer_get_layer(s_date_layer), true);
        }
    }
}

void watchface_show_centered(void) {
    if (s_time_layer && s_date_layer) {
        Layer *root_layer = window_get_root_layer(layer_get_window(text_layer_get_layer(s_time_layer)));
        GRect bounds = layer_get_bounds(root_layer);

        int time_h = NATIVE_H_LOCAL(42, 42, bounds.size.h);
        int date_h = 28; // Adjusted height for single-line 24pt font
        int total_h = time_h + date_h;
        int offset_y = (bounds.size.h - total_h) / 2;

        layer_set_frame(text_layer_get_layer(s_time_layer), GRect(0, offset_y - 6, bounds.size.w, time_h));
        layer_set_frame(text_layer_get_layer(s_date_layer), GRect(0, offset_y + time_h - 10, bounds.size.w, date_h));

        layer_set_hidden(text_layer_get_layer(s_time_layer), false);
        layer_set_hidden(text_layer_get_layer(s_date_layer), false);
    }
}

void watchface_hide(void) {
    if (s_time_layer) layer_set_hidden(text_layer_get_layer(s_time_layer), true);
    if (s_date_layer) layer_set_hidden(text_layer_get_layer(s_date_layer), true);
}

void watchface_set_date_visibility(bool visible) {
    if (s_date_layer) {
        layer_set_hidden(text_layer_get_layer(s_date_layer), !visible);
    }
}
