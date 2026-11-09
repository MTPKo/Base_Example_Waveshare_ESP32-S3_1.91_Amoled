#pragma once

#include <Arduino.h>
#include <Wire.h>

// FT3168 I2C address (touch controller)
#define TOUCH_I2C_ADDR 0x38

// FT3168 register addresses
#define FT3168_REG_MODE  0x00
#define FT3168_REG_TOUCH 0x02
#define FT3168_REG_TOUCH1_XH 0x03

// Bytes per touch point in register map
#define FT3168_BYTES_PER_POINT 6

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
 *
 * Register map (Working Mode):
 * Point 0 (TOUCH1): 0x03(XH) 0x04(XL) 0x05(YH) 0x06(YL) 0x07(Weight) 0x08(Misc)
 * Point 1 (TOUCH2): 0x09(XH) 0x0A(XL) 0x0B(YH) 0x0C(YL) 0x0D(Weight) 0x0E(Misc)
 * Point 2 (TOUCH3): 0x0F(XH) 0x10(XL) 0x11(YH) 0x12(YL) 0x13(Weight) 0x14(Misc)
 * etc.
 *
 * FT3168 native coordinates are portrait (240x536, Y=0 at bottom).
 * Display is landscape (536x240). Rotation:
 *   screenX = portraitY
 *   screenY = (239 - portraitX)
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
   * Read touch coordinates for ALL active touch points.
  *
   * @param x Pointer to store primary X coordinate
   * @param y Pointer to store primary Y coordinate
   * @return true if at least one valid touch point was read
   */
  bool getTouch(int16_t *x, int16_t *y) {
    // Read touch status (number of active touch points) from register 0x02
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

    // bits 3:0 = number of touch points (0-5)
    _point_count = status & 0x0F;
    if (_point_count == 0 || _point_count > TOUCH_MAX_POINTS) {
      _point_count = 0;
      clearAllPoints();
      return false;
    }

    // Read ALL touch point data
    // Each point is 6 bytes in the register map
    uint8_t totalBytes = _point_count * FT3168_BYTES_PER_POINT;
    uint8_t buf[TOUCH_MAX_POINTS * FT3168_BYTES_PER_POINT];  // Max 30 bytes

    _wire->beginTransmission(_addr);
    _wire->write(FT3168_REG_TOUCH1_XH);
    _wire->endTransmission(false);  // Restart, don't stop
    if (_wire->requestFrom(_addr, (uint8_t)totalBytes) != totalBytes) {
      _point_count = 0;
      clearAllPoints();
      return false;
    }
    for (int i = 0; i < totalBytes; i++) {
      buf[i] = _wire->read();
    }

    // Decode each touch point
    for (uint8_t i = 0; i < _point_count; i++) {
      // Each point is 6 bytes: [XH, XL, YH, YL, Weight, Misc]
      int base = i * FT3168_BYTES_PER_POINT;
      _points[i].active = true;
      _points[i].id = i;

      // Decode X: (XH & 0x0F) << 8 | XL
      // XH register: bit7=Event Flag, bits6-4=reserved, bits3-0=X[11:8]
      int16_t rawX = (((uint16_t)(buf[base + 0] & 0x0F)) << 8) | buf[base + 1];

      // Decode Y: (YH & 0x0F) << 8 | YL
      // YH register: bits7-4=Y[11:8], bits3-0=Touch ID
      int16_t rawY = (((uint16_t)(buf[base + 2] & 0x0F)) << 8) | buf[base + 3];

      // Rotate coordinates: FT3168 reports portrait (240x536), display is landscape (536x240)
      // screenX = portraitY, screenY = (239 - portraitX)
      _points[i].x = (TOUCH_MAX_X - 1) - rawY;
      _points[i].y =  rawX;


    }

    // Output primary coordinates via pointers
    *x = _points[0].x;
    *y = _points[0].y;

    _lastX = _points[0].x;
    _lastY = _points[0].y;

    return true;
  }

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
  // more than 2 still buggy

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
