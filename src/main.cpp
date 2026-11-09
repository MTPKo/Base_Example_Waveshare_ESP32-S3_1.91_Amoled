#include "input.h"
#include "qspi_display.h"
#include "touch_face.h"
#include "ui.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <lvgl.h>

// I2C Pins for touch controller (TP_SCL=39, TP_SDA=40)
#define I2C_SDA 40
#define I2C_SCL 39

// Buffer height for partial rendering (60 lines each)
#define BUF_HEIGHT 60

// Power management settings
#define IDLE_TIMEOUT_MS 10000 // 10 seconds
#define FADE_OUT_STEP 8       // Brightness reduction per step
#define FADE_IN_STEP 12       // Brightness increase per step
#define FADE_OUT_INTERVAL_MS 20
#define FADE_IN_INTERVAL_MS 10
#define TARGET_BRIGHTNESS 200



// Global activity flag that input.cpp can set
volatile bool g_activity_detected = false;

/* Display flushing callback for LVGL 9 */
void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  lcd.setWindow(area->x1, area->y1, w, h);
  lcd.pushPixels((uint16_t *)px_map, w * h);

  lv_display_flush_ready(disp);
}

void setup() {
  Serial.begin(115200);
  delay(2000); // Wait for Serial Monitor to connect
  Serial.println("Starting...");

  // Init I2C for touch controller
  Wire.begin(I2C_SDA, I2C_SCL);

  // Init Display
  if (!lcd.begin()) {
    Serial.println("Display init failed!");
    while (1)
      delay(100);
  }

  // Init Touch
  if (!touch_face
    .begin(Wire)) {
    Serial.println("FT3168 touch not found!");
  } else {
    Serial.println("Touch ready");
  }

  // Init LVGL
  lv_init();

  // Register tick callback
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  // Create display with LVGL 9 API
  lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);

  // Allocate double buffers in PSRAM for smooth rendering
  size_t buf_size = LCD_WIDTH * BUF_HEIGHT * sizeof(lv_color_t);
  uint8_t *buf1 = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
  uint8_t *buf2 = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);

  if (!buf1 || !buf2) {
    Serial.println("PSRAM buffer alloc failed, using internal RAM");
    static uint8_t fallback_buf[LCD_WIDTH * 20 * sizeof(lv_color_t)];
    lv_display_set_buffers(disp, fallback_buf, NULL, sizeof(fallback_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
  } else {
    lv_display_set_buffers(disp, buf1, buf2, buf_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.printf("PSRAM buffers allocated: %d bytes each\n", buf_size);
  }

  lv_display_set_flush_cb(disp, disp_flush);

  // Create pointer input device with LVGL 9 API (touch)
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, keypad_read);
  lv_indev_set_display(indev, disp);

  // Set faster polling rate for better responsiveness
  lv_timer_set_period(lv_indev_get_read_timer(indev), 20);

  // Build UI
  ui_init();

  Serial.println("Setup complete");
}

enum power_state_t {
  STATE_AWAKE,
  STATE_FADING_OUT,
  STATE_LIGHT_SLEEP,
  STATE_FADING_IN
};

static power_state_t power_state = STATE_AWAKE;
static uint8_t cur_brightness = TARGET_BRIGHTNESS;
static uint32_t last_brightness_update = 0;
static uint32_t last_activity_time = 0;

void enter_light_sleep() {
  Serial.println("Entering light sleep mode...");

  // Put display in sleep mode
  lcd.setSleep(true);

  // Light sleep with polling: sleep in short bursts and check for activity
  // This is more reliable than GPIO interrupt for I2C devices
  while (power_state == STATE_LIGHT_SLEEP) {
    // Sleep for 100ms at a time
    esp_sleep_enable_timer_wakeup(100 * 1000ULL); // 100ms in microseconds
    esp_light_sleep_start();

    // Check for touch activity after each wake
    touch_face.update();

    // Check for touch
    if (touch_face.isTouching()) {

      Serial.println("Touch activity detected, waking display!");

      // Wake display FIRST
      lcd.setSleep(false);
      Serial.println("Display sleep disabled");

      // Reinitialize I2C to ensure reliability
      Wire.begin(I2C_SDA, I2C_SCL);

      // Reinitialize Serial to restore USB CDC communication
      Serial.begin(115200);
      delay(50); // Small delay for USB re-enumeration/sync

      // Clear any pending touch data
      touch_face.update();

      // Set brightness to 0 to start fade-in from black
      cur_brightness = 0;
      lcd.setBrightness(cur_brightness);
      Serial.println("Brightness reset to 0 for fade-in");

      // Force full screen refresh
      lv_obj_invalidate(lv_screen_active());
      Serial.println("Screen invalidated");

      // Set activity flag and update timestamp
      g_activity_detected = true;
      last_activity_time = millis();
      last_brightness_update = millis();

      // Change to fading in state
      power_state = STATE_FADING_IN;
      Serial.println("State changed to FADING_IN");

      break; // Exit sleep loop
    }
  }

  Serial.println("Exited light sleep");
}

void handle_power_save() {
  uint32_t now = millis();

  // Check for activity flag set by input handler
  if (g_activity_detected) {
    last_activity_time = now;
    g_activity_detected = false;

    // If in light sleep, wake-up is handled in enter_light_sleep() after
    // esp_light_sleep_start() This handles activity during fade states
    if (power_state == STATE_FADING_OUT) {
      Serial.println("Activity during fade-out, reversing...");
      power_state = STATE_FADING_IN;
      last_brightness_update = now;
    } else if (power_state == STATE_LIGHT_SLEEP) {
      // Just came back from light sleep, start fading in
      power_state = STATE_FADING_IN;
      last_brightness_update = now;
    }
  }

  uint32_t idle_time = now - last_activity_time;

  switch (power_state) {
  case STATE_AWAKE:
    if (idle_time > IDLE_TIMEOUT_MS) {
      power_state = STATE_FADING_OUT;
      last_brightness_update = now;
      Serial.println("Idle timeout, fading out...");
    }
    break;

  case STATE_FADING_OUT:
    if (now - last_brightness_update > FADE_OUT_INTERVAL_MS) {
      if (cur_brightness > 0) {
        int next_b = (int)cur_brightness - FADE_OUT_STEP;
        cur_brightness = (next_b < 0) ? 0 : next_b;
        lcd.setBrightness(cur_brightness);
      } else {
        power_state = STATE_LIGHT_SLEEP;
        enter_light_sleep(); // Blocks until wake
        // After wake, state will be changed to FADING_IN by activity detection
      }
      last_brightness_update = now;
    }
    break;

  case STATE_LIGHT_SLEEP:
    // This state should not be reached in normal flow
    // Wake-up is handled inside enter_light_sleep()
    Serial.println("WARNING: STATE_LIGHT_SLEEP reached in state machine");
    power_state = STATE_FADING_IN;
    break;

  case STATE_FADING_IN:
    if (now - last_brightness_update > FADE_IN_INTERVAL_MS) {
      if (cur_brightness < TARGET_BRIGHTNESS) {
        int next_b = (int)cur_brightness + FADE_IN_STEP;
        cur_brightness =
            (next_b > TARGET_BRIGHTNESS) ? TARGET_BRIGHTNESS : next_b;
        lcd.setBrightness(cur_brightness);
        if (cur_brightness % 48 == 0) {
          Serial.printf("Fading in... brightness=%d\n", cur_brightness);
        }
      } else {
        power_state = STATE_AWAKE;
        Serial.println("Display fully awake");
      }
      last_brightness_update = now;
    }
    break;
  }
}


void loop() {
  // Update touch ONCE per loop iteration
  touch_face.update();

  // Handle LVGL timers (which will call keypad_read)
  lv_timer_handler();

  // Handle power management
  handle_power_save();

  delay(5);
}
