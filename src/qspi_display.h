#pragma once

#include <Arduino.h>
#include <driver/spi_master.h>

// Pin definitions for Waveshare ESP32-S3-AMOLED-1.91
#define LCD_CS 6
#define LCD_SCK 47
#define LCD_D0 18 // MOSI / SIO0
#define LCD_D1 7  // MISO / SIO1
#define LCD_D2 48 // WP / SIO2
#define LCD_D3 5  // HD / SIO3
#define LCD_RST 17

// Display dimensions
#define LCD_WIDTH 536
#define LCD_HEIGHT 240

// QSPI Settings
#define QSPI_FREQUENCY 80000000
#define QSPI_MAX_PIXELS 8192

class QSPI_Display {
public:
  QSPI_Display() : _handle(nullptr), _last_brightness(0xD0) {}

  bool begin();
  void reset();
  void setBrightness(uint8_t brightness);
  void setSleep(bool sleep);

  void writeCommand(uint8_t cmd);
  void writeData(uint8_t data);
  void writeData16(uint16_t data);
  void writeC8D8(uint8_t cmd, uint8_t data);
  void writeC8D16(uint8_t cmd, uint16_t data);

  void setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void pushPixels(uint16_t *data, uint32_t len);
  void pushColor(uint16_t color, uint32_t len);

private:
  spi_device_handle_t _handle;
  spi_transaction_ext_t _spi_tran_ext;
  spi_transaction_t *_spi_tran;

  uint8_t *_buffer;
  uint8_t _last_brightness;

  void initPanel();
  void CS_HIGH();
  void CS_LOW();
  void pollStart();
  void pollEnd();
};

// Global instance
extern QSPI_Display lcd;
