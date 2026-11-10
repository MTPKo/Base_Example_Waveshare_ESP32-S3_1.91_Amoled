# WiFi Integration Plan

## Overview
Integrate WiFi connectivity into the ESP32-S3 traminator project using the Arduino `WiFi` library. The user will be able to scan for networks, select one, enter a password, connect, view connection info, and disconnect.

## Display Context
- **Display**: ESP32-S3-AMOLED-1.91
- **Resolution**: 536x240 pixels (landscape)
- **Framework**: Arduino via PlatformIO
- **Input**: Touch (FT3168) + LVGL gridnav

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        main.cpp                              │
│  - setup(): init wifi_manager                                │
│  - loop(): handle_wifi_events() + power_save                 │
├─────────────────────────────────────────────────────────────┤
│                      wifi_manager.cpp                        │
│  - WiFi.scanNetworks() / WiFi.begin() / WiFi.disconnect()    │
│  - WiFi.status(), WiFi.localIP(), WiFi.SSID()                │
│  - Event callback (WIFI_EVENT_STA_DISCONNECTED)              │
├─────────────────────────────────────────────────────────────┤
│                       wifi_ui.cpp                            │
│  Screen 1: Network List (scrollable list of scanned networks)│
│  Screen 2: Password Input (virtual keyboard / on-screen)     │
│  Screen 3: Connection Status (IP, signal, disconnect btn)    │
├─────────────────────────────────────────────────────────────┤
│                       ui.cpp / ui.h                          │
│  - Add WiFi navigation button to main UI screen             │
│  - Screen switching via lv_obj_add_flag / clear_flag        │
└─────────────────────────────────────────────────────────────┘
```

### Screen Flow
```
[WiFi Button on Main UI]
        │
        ▼
[Screen 1: Available Networks]
  - Scan button
  - List of SSIDs with RSSI
  - [Back] button
        │ (tap network)
        ▼
[Screen 2: Enter Password]
  - SSID label
  - Password input field (on-screen keyboard)
  - [Connect] [Back] buttons
        │ (tap Connect)
        ▼
[Screen 3: Connection Status]
  - Connected / Connecting / Failed status
  - SSID, IP address, RSSI
  - [Disconnect] [Back] buttons
```

---

## 1. `src/wifi_manager.h` — WiFi Manager Class

```cpp
#pragma once
#include <Arduino.h>
#include <WiFi.h>

class WiFiManager {
public:
    void begin();
    void scan();                        // Scan for networks, returns count
    bool connect(const char* ssid, const char* passwd);
    void disconnect();
    
    bool isConnected() const;
    int8_t getRssi() const;
    String getSSID() const;
    String getIP() const;
    String getMAC() const;
    
    struct Network {
        String ssid;
        int rssi;
        uint8_t encryption;            // WiFi auth mode
    };
    const Vector<Network>& getNetworks() const { return networks_; }

private:
    void eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    
    Vector<Network> networks_;
    bool connected_ = false;
};

extern WiFiManager wifiManager;
```

### Key Methods
- `begin()` — Initializes WiFi in STA mode
- `scan()` — Calls `WiFi.scanNetworks(true)` for async scan
- `connect()` — Stores credentials in NVS, connects
- `disconnect()` — Disconnects and clears NVS credentials
- Event handler for `WIFI_EVENT_STA_DISCONNECTED` to auto-reset state

---

## 2. `src/wifi_manager.cpp` — Implementation

```cpp
#include "wifi_manager.h"
#include "nvs_flash.h"
#include "esp_wifi.h"

WiFiManager wifiManager;

void WiFiManager::begin() {
    nvs_flash_init();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    esp_event_handler_instance_t instance;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &eventHandler, this, &instance);
}
```

### Implementation Details
- Use `WiFi.scanNetworks(true)` for async scanning
- Handle `WIFI_EVENT_STA_DISCONNECTED` event to update `connected_` flag
- Use NVS to persist credentials across reboots
- RSSI threshold for filtering weak networks

---

## 3. `src/wifi_ui.h` — WiFi UI Class

```cpp
#pragma once
#include <lvgl.h>

class WiFiUI {
public:
    void init(lv_obj_t* parent);
    void show();
    void hide();
    
    enum Screen { NETWORK_LIST, PASSWORD_INPUT, CONNECTION_STATUS };

private:
    void createNetworkListScreen();
    void createPasswordInputScreen();
    void createConnectionStatusScreen();
    
    void refreshNetworkList();
    void onNetworkSelected(int index);
    void onConnectPressed();
    void onDisconnectPressed();
    void updateConnectionStatus();
    
    static void connectCallback(lv_event_t* e);
    static void disconnectCallback(lv_event_t* e);
    static void scanCallback(lv_event_t* e);
    static void networkSelectCallback(lv_event_t* e);
    static void backCallback(lv_event_t* e);

    lv_obj_t* parent_;
    lv_obj_t* screens_[3];
    Screen currentScreen_ = NETWORK_LIST;
    int selectedNetwork_ = -1;
};
```

---

## 4. `src/wifi_ui.cpp` — WiFi UI Implementation

### Screen 1: Network List (536x240)
```
┌──────────────────────────────────────────┐
│  [Scan]  WiFi Networks        [Back]    │
├──────────────────────────────────────────┤
│  📶 MyHome        -72 dBm               │
│  📶 Office5G      -55 dBm               │
│  🔒 NeighborWiFi  -80 dBm               │
│  📶 FreeWiFi      -90 dBm               │
│                                          │
└──────────────────────────────────────────┘
```

### Screen 2: Password Input (536x240)
```
┌──────────────────────────────────────────┐
│  Connect to: MyHome              [Back]  │
├──────────────────────────────────────────┤
│  Password: [________________]            │
│                                          │
│  [A] [B] [C] [D] [E] [F] [G] [H] [I]   │
│  [J] [K] [L] [M] [N] [O] [P] [Q] [R]   │
│  [S] [T] [U] [V] [W] [X] [Y] [Z] [⌫]   │
│  [1] [2] [3] [4] [5] [6] [7] [8] [9]   │
│  [0] [space]          [Connect]         │
└──────────────────────────────────────────┘
```

### Screen 3: Connection Status (536x240)
```
┌──────────────────────────────────────────┐
│  ✓ Connected                   [Back]    │
├──────────────────────────────────────────┤
│  Network:    MyHome                      │
│  IP:         192.168.1.100               │
│  Signal:     -72 dBm (Good)              │
│  MAC:        AA:BB:CC:DD:EE:FF           │
│                                          │
│  [Disconnect]                            │
└──────────────────────────────────────────┘
```

---

## 5. `src/ui.h` / `src/ui.cpp` Updates

Add WiFi navigation button to the existing 3x3 color picker grid:

In [`ui.cpp`](src/ui.cpp:95), add a 4th row with a WiFi button:
```cpp
// After the 3x3 color grid, add WiFi entry below
lv_obj_t* wifiBtn = lv_button_create(scr);
lv_obj_set_pos(wifiBtn, 0, LCD_HEIGHT - 60);  // Bottom center
lv_obj_set_size(wifiBtn, 120, 40);
lv_obj_t* wifiLabel = lv_label_create(wifiBtn);
lv_label_set_text(wifiLabel, "WiFi");
lv_obj_center(wifiLabel);
```

On tap, call `wifiUI.show()` to display the WiFi screen overlay.

---

## 6. `src/main.cpp` Updates

### In `setup()`:
```cpp
// After Serial.begin, init WiFi manager
wifiManager.begin();
```

### In `loop()`:
```cpp
void handle_wifi_events() {
    // Check connection state and update UI if needed
    if (wifiManager.isConnected() != currentConnectedState) {
        currentConnectedState = wifiManager.isConnected();
        // Trigger UI update
    }
}
```

---

## 7. `platformio.ini` Updates

No additional build flags needed — Arduino WiFi library is built-in. The existing config already supports ESP32-S3 with PSRAM.

---

## File Summary

| File | Action | Purpose |
|------|--------|---------|
| `src/wifi_manager.h` | **Create** | WiFi Manager class declaration |
| `src/wifi_manager.cpp` | **Create** | WiFi scan/connect/disconnect logic |
| `src/wifi_ui.h` | **Create** | WiFi UI class declaration |
| `src/wifi_ui.cpp` | **Create** | WiFi screens (list, password, status) |
| `src/ui.h` | **Modify** | Add WiFi UI function declarations |
| `src/ui.cpp` | **Modify** | Add WiFi button to main UI |
| `src/main.cpp` | **Modify** | Init WiFi manager, handle events |
