#include "qspi_display.h"
#include <string.h>

// Global instance
QSPI_Display lcd;

// RM67162 Commands
#define RM67162_SLPOUT 0x11
#define RM67162_DISPON 0x29
#define RM67162_CASET 0x2A
#define RM67162_PASET 0x2B
#define RM67162_RAMWR 0x2C
#define RM67162_MADCTL 0x36
#define RM67162_COLMOD 0x3A
#define RM67162_BRIGHTNESS 0x51
#define RM67162_INVON 0x21

// MADCTL bits
#define MADCTL_MY 0x80
#define MADCTL_MX 0x40
#define MADCTL_MV 0x20
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08

bool QSPI_Display::begin() {
  // Configure CS pin
  pinMode(LCD_CS, OUTPUT);
  digitalWrite(LCD_CS, HIGH);

  // Reset display
  reset();

  // Configure SPI bus
  spi_bus_config_t buscfg = {.mosi_io_num = LCD_D0,
                             .miso_io_num = LCD_D1,
                             .sclk_io_num = LCD_SCK,
                             .quadwp_io_num = LCD_D2,
                             .quadhd_io_num = LCD_D3,
                             .data4_io_num = -1,
                             .data5_io_num = -1,
                             .data6_io_num = -1,
                             .data7_io_num = -1,
                             .max_transfer_sz = (QSPI_MAX_PIXELS * 16) + 8,
                             .flags = SPICOMMON_BUSFLAG_MASTER |
                                      SPICOMMON_BUSFLAG_GPIO_PINS,
                             .intr_flags = 0};

  esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK) {
    Serial.printf("SPI bus init failed: %d\n", ret);
    return false;
  }

  // Configure SPI device
  spi_device_interface_config_t devcfg = {.command_bits = 8,
                                          .address_bits = 24,
                                          .dummy_bits = 0,
                                          .mode = 0,
                                          .duty_cycle_pos = 0,
                                          .cs_ena_pretrans = 0,
                                          .cs_ena_posttrans = 0,
                                          .clock_speed_hz = QSPI_FREQUENCY,
                                          .input_delay_ns = 0,
                                          .spics_io_num =
                                              -1, // Manual CS control
                                          .flags = SPI_DEVICE_HALFDUPLEX,
                                          .queue_size = 1,
                                          .pre_cb = nullptr,
                                          .post_cb = nullptr};

  ret = spi_bus_add_device(SPI2_HOST, &devcfg, &_handle);
  if (ret != ESP_OK) {
    Serial.printf("SPI device add failed: %d\n", ret);
    return false;
  }

  spi_device_acquire_bus(_handle, portMAX_DELAY);

  memset(&_spi_tran_ext, 0, sizeof(_spi_tran_ext));
  _spi_tran = (spi_transaction_t *)&_spi_tran_ext;

  // Allocate DMA buffer
  _buffer = (uint8_t *)heap_caps_aligned_alloc(16, QSPI_MAX_PIXELS * 2,
                                               MALLOC_CAP_DMA);
  if (!_buffer) {
    Serial.println("Buffer alloc failed");
    return false;
  }

  // Initialize display
  writeCommand(RM67162_SLPOUT);
  delay(120);
  initPanel();

  Serial.println("Display initialized");
  return true;
}

void QSPI_Display::reset() {
  pinMode(LCD_RST, OUTPUT);
  digitalWrite(LCD_RST, LOW);
  delay(20);
  digitalWrite(LCD_RST, HIGH);
  delay(150);
}

void QSPI_Display::setBrightness(uint8_t brightness) {
  _last_brightness = brightness;
  writeC8D8(RM67162_BRIGHTNESS, brightness);
}

void QSPI_Display::setSleep(bool sleep) {
  if (sleep) {
    writeCommand(0x28); // DISPOFF
    delay(20);
    writeCommand(0x10); // SLPIN
    delay(5);           // Required wait
  } else {
    writeCommand(0x11); // SLPOUT
    delay(150);         // Wait for display oscillator
    writeCommand(0x13); // Normal mode
    initPanel();
  }
}

void QSPI_Display::initPanel() {
  writeC8D8(RM67162_COLMOD, 0x55); // 16-bit color
  writeC8D8(RM67162_MADCTL,
            MADCTL_MY | MADCTL_MV |
                MADCTL_RGB); // Landscape, rotated, RGB colors
  writeC8D8(RM67162_BRIGHTNESS, _last_brightness);
  writeCommand(RM67162_DISPON);
  writeCommand(RM67162_INVON); // Turn on inversion for proper black levels
  delay(20);
}

void QSPI_Display::writeCommand(uint8_t cmd) {
  CS_LOW();
  _spi_tran_ext.base.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  _spi_tran_ext.base.cmd = 0x02;
  _spi_tran_ext.base.addr = ((uint32_t)cmd) << 8;
  _spi_tran_ext.base.tx_buffer = NULL;
  _spi_tran_ext.base.length = 0;
  pollStart();
  pollEnd();
  CS_HIGH();
}

void QSPI_Display::writeC8D8(uint8_t cmd, uint8_t data) {
  CS_LOW();
  _spi_tran_ext.base.flags =
      SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  _spi_tran_ext.base.cmd = 0x02;
  _spi_tran_ext.base.addr = ((uint32_t)cmd) << 8;
  _spi_tran_ext.base.tx_data[0] = data;
  _spi_tran_ext.base.length = 8;
  pollStart();
  pollEnd();
  CS_HIGH();
}

void QSPI_Display::setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  uint16_t x1 = x + w - 1;
  uint16_t y1 = y + h - 1;

  // Column address (CASET)
  CS_LOW();
  _spi_tran_ext.base.flags =
      SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  _spi_tran_ext.base.cmd = 0x02;
  _spi_tran_ext.base.addr = ((uint32_t)RM67162_CASET) << 8;
  _spi_tran_ext.base.tx_data[0] = x >> 8;
  _spi_tran_ext.base.tx_data[1] = x & 0xFF;
  _spi_tran_ext.base.tx_data[2] = x1 >> 8;
  _spi_tran_ext.base.tx_data[3] = x1 & 0xFF;
  _spi_tran_ext.base.length = 32;
  pollStart();
  pollEnd();
  CS_HIGH();

  // Row address (PASET)
  CS_LOW();
  _spi_tran_ext.base.addr = ((uint32_t)RM67162_PASET) << 8;
  _spi_tran_ext.base.tx_data[0] = y >> 8;
  _spi_tran_ext.base.tx_data[1] = y & 0xFF;
  _spi_tran_ext.base.tx_data[2] = y1 >> 8;
  _spi_tran_ext.base.tx_data[3] = y1 & 0xFF;
  pollStart();
  pollEnd();
  CS_HIGH();

  // Write RAMWR command to start pixel data
  writeCommand(RM67162_RAMWR);
}

void QSPI_Display::pushPixels(uint16_t *data, uint32_t len) {
  CS_LOW();

  bool first = true;
  uint32_t remaining = len;
  uint16_t *src = data;

  while (remaining > 0) {
    uint32_t chunk =
        (remaining > QSPI_MAX_PIXELS) ? QSPI_MAX_PIXELS : remaining;

    // Copy pixel data directly (no byte swap needed with BGR mode)
    memcpy(_buffer, src, chunk * 2);

    if (first) {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
      _spi_tran_ext.base.cmd = 0x32;
      _spi_tran_ext.base.addr = 0x003C00; // RAMWR with dummy
      first = false;
    } else {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                 SPI_TRANS_VARIABLE_ADDR |
                                 SPI_TRANS_VARIABLE_DUMMY;
    }

    _spi_tran_ext.base.tx_buffer = _buffer;
    _spi_tran_ext.base.length = chunk << 4; // bits = pixels * 16

    pollStart();
    pollEnd();

    src += chunk;
    remaining -= chunk;
  }

  CS_HIGH();
}

void QSPI_Display::CS_HIGH() { GPIO.out_w1ts = (1UL << LCD_CS); }

void QSPI_Display::CS_LOW() { GPIO.out_w1tc = (1UL << LCD_CS); }

void QSPI_Display::pollStart() {
  spi_device_polling_start(_handle, _spi_tran, portMAX_DELAY);
}

void QSPI_Display::pollEnd() { spi_device_polling_end(_handle, portMAX_DELAY); }
