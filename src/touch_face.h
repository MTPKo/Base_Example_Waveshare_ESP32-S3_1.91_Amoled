#pragma once

#include <Arduino.h>
#include <Wire.h>

// FT3168 I2C address (touch controller)
#define TOUCH_I2C_ADDR 0x38

// FT3168 register addresses
#define FT3168_REG_MODE  0x00
#define FT3168_REG_TOUCH 0x02
#define FT3168_REG_XY    0x03


// Display resolution for clamping
#define TOUCH_MAX_X 536
#define TOUCH_MAX_Y 240

/**
 * FT3168 capacitive touch controller driver
 * Adapted from Waveshare ESP-IDF example (touch_bsp.c)
 * Replaces the trackball class for ESP32-S3-Touch-AMOLED-1.91-M
 */

class Touch_face {
public:
  Touch_face() : _wire(nullptr), _addr(TOUCH_I2C_ADDR) {}

  bool begin(TwoWire &wire, uint8_t addr = TOUCH_I2C_ADDR) {
    _wire = &wire;
    _addr = addr;

    // Initialize I2C with touch pins (GPIO39=SCL, GPIO40=SDA)
    wire.begin(40, 39);

    // Check if FT3168 is present
    wire.beginTransmission(_addr);
    uint8_t error = wire.endTransmission();
    if (error != 0) {
      Serial.printf("FT3168 not found at 0x%02X (error: %d)\n", _addr, error);
      return false;
    }

    // Switch to normal mode (from touch_bsp.c)
    uint8_t mode = 0x00;
    wire.beginTransmission(_addr);
    wire.write(FT3168_REG_MODE);
    wire.write(mode);
    wire.endTransmission();

    Serial.println("FT3168 touch initialized");
    return true;
  }

  /**
   * Read touch coordinates
   * @param x Pointer to store X coordinate
   * @param y Pointer to store Y coordinate (already flipped for landscape)
   * @return true if valid touch data was read
   */
  bool getTouch(int16_t *x, int16_t *y) {
    uint8_t buf[4];

    // Read touch status first (register 0x02)
    // From touch_bsp.c: I2C_read_buff(I2C_ADDR_FT3168, 0x02, &data, 1);
    uint8_t status;
    _wire->beginTransmission(_addr);
    _wire->write(FT3168_REG_TOUCH);
    _wire->endTransmission(false);
    if (_wire->requestFrom(_addr, (uint8_t)1) != 1) return false;
    status = _wire->read();

    // Original touch_bsp.c checks: if(data) { ... }
    // status = number of touch points (0 = no touch, 1-5 = touch points)
    if (status == 0) return false;

    // Read 4 bytes of touch data from register 0x03
    // From touch_bsp.c: I2C_read_buff(I2C_ADDR_FT3168, 0x03, buf, 4);
    _wire->beginTransmission(_addr);
    _wire->write(FT3168_REG_XY);
    _wire->endTransmission(false);
    if (_wire->requestFrom(_addr, (uint8_t)4) != 4) return false;
    for (int i = 0; i < 4; i++) buf[i] = _wire->read();

    // Decode coordinates (from touch_bsp.c)
    *y = (((uint16_t)(buf[0] & 0x0F)) << 8) | buf[1];
    *x = -(((((uint16_t)(buf[2] & 0x0F)) << 8) | buf[3])-536);



    // Clamp to display resolution
    if (*x > TOUCH_MAX_X) *x = TOUCH_MAX_X;
    if (*y > TOUCH_MAX_Y) *y = TOUCH_MAX_Y;

    // Flip Y axis: FT3168 returns portrait-native coordinates
    // Original Waveshare touch_bsp.c: *y = EXAMPLE_LCD_V_RES - *y
    //*y = TOUCH_MAX_Y - *y;

    return true;
  }

  /**
   * Update function — kept for API compatibility
   * Returns true if touch is active
   */
  bool update() {
    int16_t x, y;
    _touchActive = getTouch(&x, &y);
    if (_touchActive) {
      _lastX = x;
      _lastY = y;
    }
    return _touchActive;
  }

  /**
   * Get last touch position
   */
  int16_t getX() const { return _lastX; }
  int16_t getY() const { return _lastY; }
  bool isTouching() const { return _touchActive; }

  // Placeholder methods for API compatibility with trackball code
  int8_t right() const { return 0; }
  int8_t left() const { return 0; }
  int8_t up() const { return 0; }
  int8_t down() const { return 0; }
  bool clicked() const { return false; }
  bool isPressed() const { return _touchActive; }

  private:
  TwoWire *_wire;
  uint8_t _addr;
  int16_t _lastX = 0;
  int16_t _lastY = 0;
  bool _touchActive = false;
};

// Global touch instance (kept as 'trackball' name for minimal code changes)
extern Touch_face touch_face;
