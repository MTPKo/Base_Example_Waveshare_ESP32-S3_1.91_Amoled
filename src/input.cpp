#include "input.h"
#include "touch_face.h"
#include <Arduino.h>
#include <lvgl.h>

// External touch instance (defined in main.cpp via trackball.h)
extern Touch_face touch_face;

// External activity flag for power management
extern volatile bool g_activity_detected;

void keypad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
  static int16_t last_x = -1;
  static int16_t last_y = -1;
  static uint8_t last_point_count = 0;

  // Read touch data directly using proven I2C path
  int16_t tmp_x, tmp_y;
  touch_face.getTouch(&tmp_x, &tmp_y);
  const uint8_t point_count = touch_face.getPointCount();

  // Build lv_indev_touch_data_t array for all active points
  static lv_indev_touch_data_t touch_data[TOUCH_MAX_POINTS];
  for (uint8_t i = 0; i < point_count; i++) {
    touch_point_t tp = touch_face.getPoint(i);
    touch_data[i].id = tp.id;
    touch_data[i].point.x = tp.x;
    touch_data[i].point.y = tp.y;
  }

  // Get primary pointer coordinates for backward compatibility
  touch_point_t primary = touch_face.getPrimaryPoint();

  // Determine touch state
  bool is_pressed = (point_count > 0);
  lv_indev_state_t current_state = is_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

  if (is_pressed) {
    // Call gesture recognizers with all touch points FIRST
    lv_indev_gesture_recognizers_update(indev, touch_data, point_count);
    lv_indev_gesture_recognizers_set_data(indev, data);

    // THEN set pointer coordinates and state (gesture functions don't overwrite these)
    data->point.x = primary.x;
    data->point.y = primary.y;
    data->state = LV_INDEV_STATE_PRESSED;

    // Flag activity for power management
    g_activity_detected = true;

    // Log touch when point count changes
    if (point_count != last_point_count) {
      Serial.printf("[TOUCH] %d point(s): id=%d x=%d y=%d\n", point_count, primary.id, primary.x, primary.y);
      last_point_count = point_count;
    }
  } else {
    if (last_state == LV_INDEV_STATE_PRESSED) {
      Serial.println("[TOUCH] released");
    }
    last_point_count = 0;
    data->point.x = last_x;
    data->point.y = last_y;
    data->state = LV_INDEV_STATE_RELEASED;
    current_state = LV_INDEV_STATE_RELEASED;
  }

  last_state = current_state;
  if (current_state == LV_INDEV_STATE_PRESSED) {
    last_x = primary.x;
    last_y = primary.y;
  }
}
