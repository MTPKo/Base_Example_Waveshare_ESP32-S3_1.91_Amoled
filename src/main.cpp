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
#define FADE_IN_STEP 40       // Brightness increase per step (faster wake response)
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
  STATE_DIMMING_TO_50,
  STATE_DIMMED_50,
  STATE_FADING_OUT,
  STATE_LIGHT_SLEEP,
  STATE_FADING_IN
};

static power_state_t power_state = STATE_AWAKE;
static uint8_t cur_brightness = TARGET_BRIGHTNESS;
static uint32_t last_brightness_update = 0;
static uint32_t last_activity_time = 0;

// Power management constants
#define DIM_TO_50_TIMEOUT_MS 10000       // 10s idle before dimming to 50%
#define SLEEP_TIMEOUT_MS 20000           // 20s idle before entering light sleep
#define DIM_50_BRIGHTNESS 80           // 50% of TARGET_BRIGHTNESS (200)

// FT3168 IRQ pin for light sleep wakeup
#define FT3168_IRQ_PIN 41

void enter_light_sleep() {

  Serial.println("Entering light sleep mode...");

  // Put display in sleep mode
  lcd.setSleep(true);

  // Configure FT3168 IRQ pin (GPIO 41) for light sleep wakeup
  // FT3168 pulls IRQ line LOW when touch occurs
  gpio_set_direction((gpio_num_t)FT3168_IRQ_PIN, GPIO_MODE_INPUT);
  gpio_pullup_dis((gpio_num_t)FT3168_IRQ_PIN);       // Disable pullup (FT3168 drives the line)
  gpio_hold_en((gpio_num_t)FT3168_IRQ_PIN);          // Hold pin state during light sleep
  esp_sleep_enable_ext0_wakeup((gpio_num_t)FT3168_IRQ_PIN, 0); // Wake on LOW
  esp_sleep_enable_timer_wakeup(10000);             // 100ms timer fallback

  while (power_state == STATE_LIGHT_SLEEP) {
    esp_light_sleep_start();
    // Reinitialize Serial/USB CDC after light sleep

    // Light sleep auto-releases GPIO hold, but be explicit
    gpio_hold_dis((gpio_num_t)FT3168_IRQ_PIN);

    // Reinitialize I2C — light sleep stops I2C peripheral clock
    Wire.begin(I2C_SDA, I2C_SCL);
    touch_face.update(); // Clear stale touch data

    if (touch_face.isTouching()) {

      Serial.begin(115200);
      delay(50); // Allow USB CDC enumeration to complete
      Serial.println("Touch activity detected, waking display!");

      // Wake display FIRST — before Serial to minimize delay
      lcd.setSleep(false);
      lcd.setBrightness(0);

      // Set activity flag and timestamp immediately
      g_activity_detected = true;
      last_activity_time = millis();
      last_brightness_update = millis();

      // Change to fading in state
      power_state = STATE_FADING_IN;

      // Force LVGL redraw IMMEDIATELY — don't wait for next timer handler cycle
      // This reduces the UI response latency by ~10-20ms
      lv_timer_handler();

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
    } else if (power_state == STATE_DIMMED_50 || power_state == STATE_DIMMING_TO_50) {
      // Woke from dimmed state, start fading in
      Serial.println("Activity during dimmed state, fading in...");
      power_state = STATE_FADING_IN;
      last_brightness_update = now;
    }
  }

  uint32_t idle_time = now - last_activity_time;

  switch (power_state) {
  case STATE_AWAKE:
    if (idle_time > DIM_TO_50_TIMEOUT_MS) {
      power_state = STATE_DIMMING_TO_50;
      last_brightness_update = now;
      Serial.println("Idle 10s, dimming to 50%...");
    }
    break;

  case STATE_DIMMING_TO_50:
    // Smoothly dim from current brightness to 50%
    if (now - last_brightness_update > FADE_OUT_INTERVAL_MS) {
      if (cur_brightness > DIM_50_BRIGHTNESS) {
        int next_b = (int)cur_brightness - FADE_OUT_STEP;
        cur_brightness = (next_b < DIM_50_BRIGHTNESS) ? DIM_50_BRIGHTNESS : next_b;
        lcd.setBrightness(cur_brightness);
        delay(2); // Let display stabilize after brightness change
      } else {
        power_state = STATE_DIMMED_50;
        last_brightness_update = now;
        Serial.println("Display dimmed to 50%");
      }
      last_brightness_update = now;
    }
    break;

  case STATE_DIMMED_50:
    // If idle for another 10s (20s total), fade to full off and enter sleep
    if (idle_time > SLEEP_TIMEOUT_MS) {
      power_state = STATE_FADING_OUT;
      last_brightness_update = now;
      Serial.println("Idle 20s, fading out to sleep...");
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
