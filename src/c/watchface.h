/*
 * Pebble Roon Remote - Watchface Module
 * Copyright (c) 2026 J_B
 */

#pragma once
#include <pebble.h>

void watchface_init(Layer *root_layer, bool is_light_mode, GFont time_font);
void watchface_deinit(void);
void watchface_tick_handler(struct tm *tick_time);
void watchface_set_theme(bool is_light_mode);
void watchface_show(int16_t vertical_offset);
void watchface_show_centered(void);
void watchface_hide(void);
void watchface_set_date_visibility(bool visible);
