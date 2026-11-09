#pragma once

#include <lvgl.h>

/**
 * Trackball keypad read callback for LVGL 9
 * Handles 4-way navigation with 90-degree counter-clockwise rotation
 */
void keypad_read(lv_indev_t *indev, lv_indev_data_t *data);
