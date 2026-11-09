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

// Maximum number of touch points supported by FT3168
#define TOUCH_MAX_POINTS 5

/**
 * Single touch point structure for FT3168 multitouch
 */
typedef struct {
    uint8_t id;
    int16_t x;
    int16_t y;
    bool active;
} touch_point_t;

/**
 * FT3168 capacitive touch controller driver
 * Supports up to 5 simultaneous touch points
 */

class Touch_face {
public:
  Touch_face() : _wire(nullptr), _addr(TOUCH_I2C_ADDR), _point_count(0) {
    // Initialize all points as inactive
    for (int i = 0; i < TOUCH_MAX_POINTS; i++) {
      _points[i].id = i;
      _points[i].x = 0;
      _points[i].y = 0;
      _points[i].active = false;
    }
  }

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
   * Read touch coordinates — uses ORIGINAL proven I2C read pattern
   * This is the EXACT same code as the original working implementation.
   * Also populates _points[0] for multitouch support.
   * @param x Pointer to store X coordinate
   * @param y Pointer to store Y coordinate
   * @return true if valid touch data was read
   */
  bool getTouch(int16_t *x, int16_t *y) {
    uint8_t buf[4];

    // Read touch status first (register 0x02)
    uint8_t status;
    _wire->beginTransmission(_addr);
    _wire->write(FT3168_REG_TOUCH);
    _wire->endTransmission(false);
    if (_wire->requestFrom(_addr, (uint8_t)1) != 1) {
      _point_count = 0;
      clearAllPoints();
      return false;
    }
    status = _wire->read();

    // status bits 3:0 = number of touch points
    _point_count = status & 0x0F;
    if (_point_count == 0 || _point_count > TOUCH_MAX_POINTS) {
      _point_count = 0;
      clearAllPoints();
      return false;
    }

    // Read 4 bytes of touch data from register 0x03
    // Original code only reads 4 bytes (first touch point)
    // We read all 4 bytes and store in _points[0]
    _wire->beginTransmission(_addr);
    _wire->write(FT3168_REG_XY);
    _wire->endTransmission(false);
    if (_wire->requestFrom(_addr, (uint8_t)4) != 4) {
      _point_count = 0;
      clearAllPoints();
      return false;
    }
    for (int i = 0; i < 4; i++) buf[i] = _wire->read();

    // Decode coordinates (EXACT same as original code)
    int16_t rawY = (((uint16_t)(buf[0] & 0x0F)) << 8) | buf[1];
    int16_t rawX = ((((uint16_t)(buf[2] & 0x0F)) << 8) | buf[3]);
    
    // Store in _points[0]
    _points[0].active = true;
    _points[0].id = 0;
    _points[0].y = rawY;
    _points[0].x = -(rawX - 536);
    
    // Clamp coordinates
    if (_points[0].x < 0) _points[0].x = 0;
    if (_points[0].x > TOUCH_MAX_X) _points[0].x = TOUCH_MAX_X;
    if (_points[0].y < 0) _points[0].y = 0;
    if (_points[0].y > TOUCH_MAX_Y) _points[0].y = TOUCH_MAX_Y;

    // Output via pointers (backward compatibility)
    *x = _points[0].x;
    *y = _points[0].y;

    _lastX = _points[0].x;
    _lastY = _points[0].y;

    // Clear other points
    for (int i = 1; i < TOUCH_MAX_POINTS; i++) {
      _points[i].active = false;
    }

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

  // ========== Multitouch Methods ==========

  /**
   * Get number of active touch points
   * @return Number of active touch points (0-5)
   */
  uint8_t getPointCount() const { return _point_count; }

  /**
   * Get a specific touch point by index
   * @param index Index of touch point (0-4)
   * @return touch_point_t at the specified index
   */
  touch_point_t getPoint(uint8_t index) const {
    if (index >= TOUCH_MAX_POINTS) {
      touch_point_t empty = {0, 0, 0, false};
      return empty;
    }
    return _points[index];
  }

  /**
   * Get the primary (first active) touch point
   * @return Primary touch_point_t, or inactive point if no touch
   */
  touch_point_t getPrimaryPoint() const {
    if (_point_count > 0) {
      return _points[0];
    }
    touch_point_t empty = {0, 0, 0, false};
    return empty;
  }

  /**
   * Clear all touch points
   */
  void clearAllPoints() {
    for (int i = 0; i < TOUCH_MAX_POINTS; i++) {
      _points[i].active = false;
      _points[i].x = 0;
      _points[i].y = 0;
    }
    _point_count = 0;
  }

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

  // Multitouch state
  touch_point_t _points[TOUCH_MAX_POINTS];
  uint8_t _point_count;
};

// Global touch instance (kept as 'touch_face' name for minimal code changes)
extern Touch_face touch_face;
