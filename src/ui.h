#pragma once

#include <lvgl.h>

/**
 * Initialize the 3x3 color picker UI
 * Creates a grid of color buttons with trackball navigation
 */
void ui_init();

/**
 * Update visual touch indicator dots based on current touch data
 * Should be called from loop() after touch_face.update()
 */
void ui_update_touch_indicators();
