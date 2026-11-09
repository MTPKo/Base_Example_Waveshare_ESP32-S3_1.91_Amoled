#include "input.h"
#include "touch_face.h"
#include <Arduino.h>

// External touch instance (defined in main.cpp via trackball.h)
extern Touch_face touch_face;

// External activity flag for power management
extern volatile bool g_activity_detected;

// Touch debounce: ignore touch events for this many ms after wake
#define TOUCH_DEBOUNCE_MS 200

void keypad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  static uint32_t last_touch_time = 0;
  static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
  static int16_t last_x = -1;
  static int16_t last_y = -1;

  int16_t x, y;

  // Read touch coordinates
  if (touch_face.getTouch(&x, &y)) {
    Serial.printf("[TOUCH] x=%d, y=%d\n", x, y);
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
    last_touch_time = millis();
    last_state = LV_INDEV_STATE_PRESSED;
    last_x = x;
    last_y = y;

    // Flag activity for power management
    g_activity_detected = true;
  } else {
    if (last_state == LV_INDEV_STATE_PRESSED) {
      Serial.println("[TOUCH] released");
    }
    data->point.x = last_x;
    data->point.y = last_y;
    data->state = LV_INDEV_STATE_RELEASED;
    last_state = LV_INDEV_STATE_RELEASED;
  }
}

