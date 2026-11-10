#include "wifi_ui.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <string.h>

// Global instance
WiFiUI wifiUI;

// Color definitions for dark theme
#define BG_COLOR lv_color_hex(0x1a1a2e)
#define BTN_COLOR lv_color_hex(0x2d2d44)
#define ACCENT_COLOR lv_palette_main(LV_PALETTE_BLUE)
#define TXT_COLOR lv_color_white()
#define HDR_BG lv_color_hex(0x252541)

// Screen dimensions
#define SCREEN_W 536
#define SCREEN_H 240
#define HEADER_H 60

// Simple ASCII symbols (no emojis to avoid compiler issues)
static const char* ICON_UNLOCKED = "[O]";     // Open network
static const char* ICON_LOCKED = "[+]";       // WPA2 secured
static const char* ICON_LOCK_NEW = "[#]";     // WPA3 secured
static const char* ICON_QUESTION = "[?]";     // Unknown encryption
static const char* ICON_SIGNAL_STRONG = "|===|";
static const char* ICON_SIGNAL_GOOD = "|==|";
static const char* ICON_SIGNAL_FAIR = "|=|";
static const char* ICON_SIGNAL_WEAK = "|.|";
static const char* ICON_SIGNAL_NONE = "| |";
static const char* ICON_SCAN_TXT = "[R]";     // Scan
static const char* ICON_CHECK = "[OK]";       // Connected
static const char* ICON_CROSS = "[NO]";       // Disconnected
static const char* ICON_CONNECTING = "[..]";  // Connecting
static const char* ICON_BACK_TXT = "<-";      // Back arrow
static const char* BULLET = ".";              // Password bullet

// ============================================================
// init - Initialize the WiFi UI overlay
// ============================================================
void WiFiUI::init(lv_obj_t* parent) {
    parent_ = parent;
    currentScreen_ = NETWORK_LIST;
    selectedNetworkIndex_ = 0;
    passwordVisible_ = false;
    keyboard_container_ = nullptr;
    password_display_ = nullptr;
    currentHighlight_ = nullptr;
    
    for (int i = 0; i < 3; i++) {
        screens_[i] = nullptr;
    }
    
    // Create all three screens
    createNetworkListScreen();
    createPasswordInputScreen();
    createConnectionStatusScreen();
    
    // Show network list by default
    showScreen(NETWORK_LIST);
    
    Serial.println("[WiFiUI] Initialized");
}

// ============================================================
// show - Show the WiFi UI
// ============================================================
void WiFiUI::show() {
    showScreen(NETWORK_LIST);
}

// ============================================================
// handle - Handle periodic tasks (scan completion, UI updates)
// ============================================================
void WiFiUI::handle() {
    // Check if a scan completed
    if (wifiManager.isScanning()) {
        // Still scanning, do nothing
        return;
    }
    
    if (!network_items_.empty() || wifiManager.getNetworks().empty()) {
        // If we were showing network list and scan completed, refresh
        if (currentScreen_ == NETWORK_LIST) {
            refreshNetworkList();
        }
    }
    
    // Update connection status if connected
    if (wifiManager.isConnected() && currentScreen_ != CONNECTION_STATUS) {
        // Optionally show connection screen when connected
    }
}

// ============================================================
// hide - Hide all WiFi screens
// ============================================================
void WiFiUI::hide() {
    for (int i = 0; i < 3; i++) {
        if (screens_[i]) {
            lv_obj_add_flag(screens_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (keyboard_container_) {
        lv_obj_add_flag(keyboard_container_, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================================
// showScreen - Switch between screens
// ============================================================
void WiFiUI::showScreen(Screen screen) {
    // Hide all screens
    for (int i = 0; i < 3; i++) {
        if (screens_[i]) {
            lv_obj_add_flag(screens_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // Show the requested screen
    if (screen < 3 && screens_[screen]) {
        lv_obj_clear_flag(screens_[screen], LV_OBJ_FLAG_HIDDEN);
        currentScreen_ = screen;
    }
    
    // Hide keyboard when switching screens
    if (keyboard_container_) {
        lv_obj_add_flag(keyboard_container_, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================================
// refreshNetworkList - Refresh after scan completes
// ============================================================
void WiFiUI::refreshNetworkList() {
    if (currentScreen_ != NETWORK_LIST || screens_[NETWORK_LIST] == nullptr) {
        return;
    }
    
    // Clear existing items
    for (auto item : network_items_) {
        lv_obj_del(item);
    }
    network_items_.clear();
    
    const auto& networks = wifiManager.getNetworks();
    
    // Content area should be second child (after header)
    lv_obj_t* header = lv_obj_get_child(screens_[NETWORK_LIST], 0);
    lv_obj_t* content = nullptr;
    if (header) {
        content = lv_obj_get_child(screens_[NETWORK_LIST], 1);
    }
    
    if (!content) {
        Serial.println("[WiFiUI] Warning: Content area not found");
        return;
    }
    
    // Clear existing items from content
    while (lv_obj_get_child_count(content) > 0) {
        lv_obj_del(lv_obj_get_child(content, 0));
    }
    
    // Add network items
    for (int i = 0; i < (int)networks.size(); i++) {
        lv_obj_t* item = createNetworkItem(
            networks[i].ssid.c_str(), 
            networks[i].rssi, 
            networks[i].encryption, 
            content, 
            i
        );
        network_items_.push_back(item);
    }
    
    if (networks.empty()) {
        lv_obj_t* lbl = lv_label_create(content);
        lv_label_set_text(lbl, "No networks found");
        lv_obj_set_pos(lbl, (SCREEN_W / 2) - 60, HEADER_H + 80);
        lv_obj_set_style_text_color(lbl, TXT_COLOR, 0);
    }
    
    Serial.printf("[WiFiUI] Refreshed network list: %d networks\n", (int)networks.size());
}

// ============================================================
// updateConnectionStatus - Update connection status screen
// ============================================================
void WiFiUI::updateConnectionStatus() {
    if (currentScreen_ != CONNECTION_STATUS || screens_[CONNECTION_STATUS] == nullptr) {
        return;
    }
    
    // Update status label - it's a direct child of screen at index 2
    lv_obj_t* statusLbl = lv_obj_get_child(screens_[CONNECTION_STATUS], 2);
    if (statusLbl) {
        if (wifiManager.isConnected()) {
            lv_label_set_text(statusLbl, "[OK] Connected");
            lv_obj_set_style_text_color(statusLbl, lv_color_hex(0x00ff88), 0);
        } else {
            lv_label_set_text(statusLbl, "[NO] Disconnected");
            lv_obj_set_style_text_color(statusLbl, TXT_COLOR, 0);
        }
    }
}

// ============================================================
// createStyledButton - Create a styled button
// ============================================================
lv_obj_t* WiFiUI::createStyledButton(const char* text, uint32_t bgColor, uint32_t textColor, 
                                      lv_obj_t* parent, lv_coord_t col, lv_coord_t col_span) {
    (void)col;
    (void)col_span;
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 36);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bgColor), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(textColor), 0);
    
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    
    return btn;
}

// ============================================================
// createNetworkItem - Create a network list item
// ============================================================
lv_obj_t* WiFiUI::createNetworkItem(const char* ssid, int rssi, uint8_t encryption, 
                                     lv_obj_t* parent, int index) {
    lv_obj_t* item = lv_button_create(parent);
    lv_obj_set_size(item, (lv_coord_t)(SCREEN_W - 20), 44);
    lv_obj_set_pos(item, 10, 0);
    lv_obj_set_style_bg_color(item, BTN_COLOR, 0);
    lv_obj_set_style_radius(item, 8, 0);
    lv_obj_set_style_border_width(item, 0, 0);
    
    // Set user data for index
    lv_obj_set_user_data(item, (void*)(intptr_t)index);
    
    // Add click handler
    lv_obj_add_event_cb(item, networkSelectCallback, LV_EVENT_CLICKED, &wifiUI);
    
    // Use flex layout for horizontal arrangement
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(item, 5, 0);
    lv_obj_set_style_pad_all(item, 8, 0);
    
    // Signal bars
    lv_obj_t* sigLbl = lv_label_create(item);
    lv_label_set_text(sigLbl, getSignalBars(rssi));
    lv_obj_set_style_text_color(sigLbl, TXT_COLOR, 0);
    
    // SSID
    lv_obj_t* ssidLbl = lv_label_create(item);
    lv_label_set_text(ssidLbl, ssid);
    lv_obj_set_width(ssidLbl, (lv_coord_t)(SCREEN_W - 180));
    lv_obj_set_style_text_color(ssidLbl, TXT_COLOR, 0);
    lv_label_set_long_mode(ssidLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    
    // RSSI + encryption
    lv_obj_t* infoLbl = lv_label_create(item);
    char infoText[48];
    snprintf(infoText, sizeof(infoText), "%s %d dBm", getEncryptionIcon(encryption), rssi);
    lv_label_set_text(infoLbl, infoText);
    lv_obj_set_style_text_color(infoLbl, lv_color_hex(0x8888aa), 0);
    
    return item;
}

// ============================================================
// getEncryptionIcon - Get encryption icon string
// ============================================================
const char* WiFiUI::getEncryptionIcon(uint8_t encryption) {
    switch (encryption) {
        case WIFI_AUTH_OPEN:
            return ICON_UNLOCKED;
        case WIFI_AUTH_WPA2_PSK:
            return ICON_LOCKED;
        case WIFI_AUTH_WPA3_PSK:
            return ICON_LOCK_NEW;
        default:
            return ICON_QUESTION;
    }
}

// ============================================================
// getSignalBars - Get signal bars string based on RSSI
// ============================================================
const char* WiFiUI::getSignalBars(int rssi) {
    if (rssi >= -50) return ICON_SIGNAL_STRONG;
    if (rssi >= -60) return ICON_SIGNAL_GOOD;
    if (rssi >= -70) return ICON_SIGNAL_FAIR;
    if (rssi >= -80) return ICON_SIGNAL_WEAK;
    return ICON_SIGNAL_NONE;
}

// ============================================================
// createNetworkListScreen - Create the network list screen
// ============================================================
void WiFiUI::createNetworkListScreen() {
    // Create screen
    lv_obj_t* screen = lv_obj_create(parent_);
    screens_[NETWORK_LIST] = screen;
    lv_obj_set_size(screen, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, BG_COLOR, 0);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    
    // Header bar
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_style_bg_color(header, HDR_BG, 0);
    lv_obj_set_style_radius(header, 0, 0);
    
    // Title label
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "WiFi Networks");
    lv_obj_set_pos(title, (SCREEN_W / 2) - 50, (HEADER_H / 2) - 10);
    lv_obj_set_style_text_color(title, TXT_COLOR, 0);
    
    // Scan button (left)
    lv_obj_t* scanBtn = lv_button_create(header);
    lv_obj_set_size(scanBtn, 70, 30);
    lv_obj_set_pos(scanBtn, 10, 5);
    lv_obj_set_style_bg_color(scanBtn, ACCENT_COLOR, 0);
    lv_obj_set_style_radius(scanBtn, 6, 0);
    lv_obj_add_event_cb(scanBtn, scanCallback, LV_EVENT_CLICKED, &wifiUI);
    
    lv_obj_t* scanLbl = lv_label_create(scanBtn);
    lv_label_set_text(scanLbl, "[R] Scan");
    lv_obj_center(scanLbl);
    
    // Back button (right)
    lv_obj_t* backBtn = lv_button_create(header);
    lv_obj_set_size(backBtn, 70, 30);
    lv_obj_set_pos(backBtn, (lv_coord_t)(SCREEN_W - 80), 5);
    lv_obj_set_style_bg_color(backBtn, BTN_COLOR, 0);
    lv_obj_set_style_radius(backBtn, 6, 0);
    lv_obj_add_event_cb(backBtn, backCallback, LV_EVENT_CLICKED, &wifiUI);
    
    lv_obj_t* backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, "<- Back");
    lv_obj_center(backLbl);
    
    // Content area (scrollable list)
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_W, (lv_coord_t)(SCREEN_H - HEADER_H));
    lv_obj_set_pos(content, 0, HEADER_H);
    lv_obj_set_style_bg_color(content, BG_COLOR, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    // Hide scrollbar by setting opa to 0
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_row(content, 5, 0);
    
    // Scan initial network list if empty
    if (wifiManager.getNetworks().empty() && !wifiManager.isScanning()) {
        Serial.println("[WiFiUI] Auto-scanning networks on init");
        wifiManager.scan();
    }
}

// ============================================================
// createPasswordInputScreen - Create password input screen
// ============================================================
void WiFiUI::createPasswordInputScreen() {
    lv_obj_t* screen = lv_obj_create(parent_);
    screens_[PASSWORD_INPUT] = screen;
    lv_obj_set_size(screen, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, BG_COLOR, 0);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    
    // Header bar
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_style_bg_color(header, HDR_BG, 0);
    
    // SSID label
    lv_obj_t* ssidLbl = lv_label_create(header);
    lv_obj_set_pos(ssidLbl, 10, 5);
    lv_obj_set_style_text_color(ssidLbl, TXT_COLOR, 0);
    
    // Back button
    lv_obj_t* backBtn = lv_button_create(header);
    lv_obj_set_size(backBtn, 70, 30);
    lv_obj_set_pos(backBtn, (lv_coord_t)(SCREEN_W - 80), 5);
    lv_obj_set_style_bg_color(backBtn, BTN_COLOR, 0);
    lv_obj_set_style_radius(backBtn, 6, 0);
    lv_obj_add_event_cb(backBtn, backCallback, LV_EVENT_CLICKED, &wifiUI);
    
    lv_obj_t* backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, "<- Back");
    lv_obj_center(backLbl);
    
    // Password display (masked)
    password_display_ = lv_label_create(screen);
    lv_obj_set_pos(password_display_, 40, (lv_coord_t)(HEADER_H + 20));
    lv_obj_set_style_text_color(password_display_, TXT_COLOR, 0);
    lv_label_set_text(password_display_, "Password: ..........");
    
    // Connect button
    lv_obj_t* connectBtn = lv_button_create(screen);
    lv_obj_set_size(connectBtn, 120, 40);
    lv_obj_set_pos(connectBtn, ((lv_coord_t)(SCREEN_W / 2)) - 60, (lv_coord_t)(HEADER_H + 70));
    lv_obj_set_style_bg_color(connectBtn, ACCENT_COLOR, 0);
    lv_obj_set_style_radius(connectBtn, 8, 0);
    lv_obj_add_event_cb(connectBtn, connectCallback, LV_EVENT_CLICKED, &wifiUI);
    
    lv_obj_t* connectLbl = lv_label_create(connectBtn);
    lv_label_set_text(connectLbl, "Connect");
    lv_obj_center(connectLbl);
    
    // Create on-screen keyboard
    createOnScreenKeyboard();
}

// ============================================================
// createConnectionStatusScreen - Create connection status screen
// ============================================================
void WiFiUI::createConnectionStatusScreen() {
    lv_obj_t* screen = lv_obj_create(parent_);
    screens_[CONNECTION_STATUS] = screen;
    lv_obj_set_size(screen, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, BG_COLOR, 0);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    
    // Header bar (child 0)
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_style_bg_color(header, HDR_BG, 0);
    
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Connection Status");
    lv_obj_set_pos(title, ((lv_coord_t)(SCREEN_W / 2)) - 60, (lv_coord_t)(HEADER_H / 2) - 10);
    lv_obj_set_style_text_color(title, TXT_COLOR, 0);
    
    // Back button
    lv_obj_t* backBtn = lv_button_create(header);
    lv_obj_set_size(backBtn, 70, 30);
    lv_obj_set_pos(backBtn, (lv_coord_t)(SCREEN_W - 80), 5);
    lv_obj_set_style_bg_color(backBtn, BTN_COLOR, 0);
    lv_obj_set_style_radius(backBtn, 6, 0);
    lv_obj_add_event_cb(backBtn, backCallback, LV_EVENT_CLICKED, &wifiUI);
    
    lv_obj_t* backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, "<- Back");
    lv_obj_center(backLbl);
    
    // Status indicator (child 2)
    lv_obj_t* statusIcon = lv_label_create(screen);
    lv_obj_set_pos(statusIcon, ((lv_coord_t)(SCREEN_W / 2)) - 60, (lv_coord_t)(HEADER_H + 20));
    lv_obj_set_style_text_color(statusIcon, TXT_COLOR, 0);
    lv_label_set_text(statusIcon, "[..] Connecting...");
    
    // Info box (child 3)
    lv_obj_t* infoBox = lv_obj_create(screen);
    lv_obj_set_size(infoBox, (lv_coord_t)(SCREEN_W - 40), 100);
    lv_obj_set_pos(infoBox, 20, (lv_coord_t)(HEADER_H + 70));
    lv_obj_set_style_bg_color(infoBox, BTN_COLOR, 0);
    lv_obj_set_style_radius(infoBox, 8, 0);
    lv_obj_set_style_border_width(infoBox, 1, 0);
    
    // Info labels
    lv_coord_t y = (lv_coord_t)(HEADER_H + 80);
    
    lv_obj_t* ssidLbl = lv_label_create(screen);
    lv_obj_set_pos(ssidLbl, 30, y);
    lv_label_set_text(ssidLbl, "SSID: N/A");
    lv_obj_set_style_text_color(ssidLbl, lv_color_hex(0x8888aa), 0);
    
    y += 20;
    
    lv_obj_t* ipLbl = lv_label_create(screen);
    lv_obj_set_pos(ipLbl, 30, y);
    lv_label_set_text(ipLbl, "IP: N/A");
    lv_obj_set_style_text_color(ipLbl, lv_color_hex(0x8888aa), 0);
    
    y += 20;
    
    lv_obj_t* macLbl = lv_label_create(screen);
    lv_obj_set_pos(macLbl, 30, y);
    lv_label_set_text(macLbl, "MAC: N/A");
    lv_obj_set_style_text_color(macLbl, lv_color_hex(0x8888aa), 0);
    
    // Disconnect button (child 7)
    lv_obj_t* disconnectBtn = lv_button_create(screen);
    lv_obj_set_size(disconnectBtn, 130, 40);
    lv_obj_set_pos(disconnectBtn, ((lv_coord_t)(SCREEN_W / 2)) - 65, (lv_coord_t)(SCREEN_H - 70));
    lv_obj_set_style_bg_color(disconnectBtn, lv_color_hex(0xcc3333), 0);
    lv_obj_set_style_radius(disconnectBtn, 8, 0);
    lv_obj_add_event_cb(disconnectBtn, disconnectCallback, LV_EVENT_CLICKED, &wifiUI);
    
    lv_obj_t* disconnectLbl = lv_label_create(disconnectBtn);
    lv_label_set_text(disconnectLbl, "Disconnect");
    lv_obj_center(disconnectLbl);
}

// ============================================================
// createOnScreenKeyboard - Create the on-screen QWERTY keyboard
// ============================================================
void WiFiUI::createOnScreenKeyboard() {
    // Create keyboard container
    keyboard_container_ = lv_obj_create(parent_);
    lv_obj_set_size(keyboard_container_, SCREEN_W, 140);
    lv_obj_set_pos(keyboard_container_, 0, (lv_coord_t)(SCREEN_H - 140));
    lv_obj_set_style_bg_color(keyboard_container_, lv_color_hex(0x151528), 0);
    lv_obj_add_flag(keyboard_container_, LV_OBJ_FLAG_HIDDEN);
    
    // Main keyboard flex container
    lv_obj_t* kbFlex = lv_obj_create(keyboard_container_);
    lv_obj_set_size(kbFlex, (lv_coord_t)(SCREEN_W - 10), 130);
    lv_obj_set_pos(kbFlex, 5, 5);
    lv_obj_set_style_bg_opa(kbFlex, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kbFlex, 0, 0);
    lv_obj_set_flex_flow(kbFlex, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(kbFlex, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(kbFlex, 3, 0);
    
    // Helper lambda to create a key button
    auto createKey = [this](lv_obj_t* parent, char keyChar, lv_color_t bgCol, const char* label) -> lv_obj_t* {
        lv_obj_t* keyBtn = lv_button_create(parent);
        lv_obj_set_size(keyBtn, 28, 22);
        lv_obj_set_style_bg_color(keyBtn, bgCol, 0);
        lv_obj_set_style_radius(keyBtn, 4, 0);
        lv_obj_set_style_border_width(keyBtn, 0, 0);
        lv_obj_add_event_cb(keyBtn, charCallback, LV_EVENT_CLICKED, &wifiUI);
        lv_obj_set_user_data(keyBtn, (void*)(intptr_t)keyChar);
        
        lv_obj_t* keyLbl = lv_label_create(keyBtn);
        if (label) {
            lv_label_set_text(keyLbl, label);
        } else {
            char tmp[2] = {keyChar, 0};
            lv_label_set_text(keyLbl, tmp);
        }
        lv_obj_center(keyLbl);
        lv_obj_set_style_text_color(keyLbl, TXT_COLOR, 0);
        
        return keyBtn;
    };
    
    // Row 1: QWERTYUIOP
    const char* row1[] = {"Q","W","E","R","T","Y","U","I","O","P",nullptr};
    for (int i = 0; row1[i]; i++) {
        createKey(kbFlex, row1[i][0], BTN_COLOR, row1[i]);
    }
    
    // Row 2: ASDFGHJKL
    const char* row2[] = {"A","S","D","F","G","H","J","K","L",nullptr};
    for (int i = 0; row2[i]; i++) {
        createKey(kbFlex, row2[i][0], BTN_COLOR, row2[i]);
    }
    
    // Row 3: ZXCVBNM,.
    const char* row3[] = {"Z","X","C","V","B","N","M",",",".",nullptr};
    for (int i = 0; row3[i]; i++) {
        createKey(kbFlex, row3[i][0], BTN_COLOR, row3[i]);
    }
    
    // Row 4: Numbers
    const char* row4[] = {"1","2","3","4","5","6","7","8","9","0",nullptr};
    for (int i = 0; row4[i]; i++) {
        createKey(kbFlex, row4[i][0], BTN_COLOR, row4[i]);
    }
    
    // Row 5: Space, Backspace, Done
    // Space
    lv_obj_t* spaceBtn = lv_button_create(kbFlex);
    lv_obj_set_size(spaceBtn, 80, 22);
    lv_obj_set_style_bg_color(spaceBtn, BTN_COLOR, 0);
    lv_obj_set_style_radius(spaceBtn, 4, 0);
    lv_obj_set_style_border_width(spaceBtn, 0, 0);
    lv_obj_add_event_cb(spaceBtn, charCallback, LV_EVENT_CLICKED, &wifiUI);
    lv_obj_set_user_data(spaceBtn, (void*)(intptr_t)' ');
    
    lv_obj_t* spaceLbl = lv_label_create(spaceBtn);
    lv_label_set_text(spaceLbl, "Space");
    lv_obj_center(spaceLbl);
    lv_obj_set_style_text_color(spaceLbl, TXT_COLOR, 0);
    
    // Backspace
    lv_obj_t* bsBtn = lv_button_create(kbFlex);
    lv_obj_set_size(bsBtn, 40, 22);
    lv_obj_set_style_bg_color(bsBtn, BTN_COLOR, 0);
    lv_obj_set_style_radius(bsBtn, 4, 0);
    lv_obj_set_style_border_width(bsBtn, 0, 0);
    lv_obj_add_event_cb(bsBtn, charCallback, LV_EVENT_CLICKED, &wifiUI);
    lv_obj_set_user_data(bsBtn, (void*)(intptr_t)'\b'); // Backspace
    
    lv_obj_t* bsLbl = lv_label_create(bsBtn);
    lv_label_set_text(bsLbl, "DEL");
    lv_obj_center(bsLbl);
    lv_obj_set_style_text_color(bsLbl, TXT_COLOR, 0);
    
    // Done (auto-connects)
    lv_obj_t* doneBtn = lv_button_create(kbFlex);
    lv_obj_set_size(doneBtn, 50, 22);
    lv_obj_set_style_bg_color(doneBtn, ACCENT_COLOR, 0);
    lv_obj_set_style_radius(doneBtn, 4, 0);
    lv_obj_set_style_border_width(doneBtn, 0, 0);
    lv_obj_add_event_cb(doneBtn, charCallback, LV_EVENT_CLICKED, &wifiUI);
    lv_obj_set_user_data(doneBtn, (void*)(intptr_t)'\r'); // Done
    
    lv_obj_t* doneLbl = lv_label_create(doneBtn);
    lv_label_set_text(doneLbl, "Done");
    lv_obj_center(doneLbl);
    lv_obj_set_style_text_color(doneLbl, TXT_COLOR, 0);
}

// ============================================================
// Callback: network select
// ============================================================
void WiFiUI::networkSelectCallback(lv_event_t* e) {
    WiFiUI* wui = (WiFiUI*)lv_event_get_user_data(e);
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(obj);
    
    if (index >= 0 && index < (int)wifiManager.getNetworks().size()) {
        wui->showPasswordScreen(index);
    }
}

// ============================================================
// Callback: scan
// ============================================================
void WiFiUI::scanCallback(lv_event_t* e) {
    WiFiUI* wui = (WiFiUI*)lv_event_get_user_data(e);
    (void)wui;
    if (!wifiManager.isScanning() && !wifiManager.isConnected()) {
        Serial.println("[WiFiUI] Starting scan...");
        wifiManager.scan();
        Serial.println("[WiFiUI] Scan started, waiting for completion...");
    }
}

// ============================================================
// Callback: connect
// ============================================================
void WiFiUI::connectCallback(lv_event_t* e) {
    WiFiUI* wui = (WiFiUI*)lv_event_get_user_data(e);
    const auto& networks = wifiManager.getNetworks();
    
    if (wui->selectedNetworkIndex_ < 0 || 
        wui->selectedNetworkIndex_ >= (int)networks.size()) {
        Serial.println("[WiFiUI] Error: Invalid network index");
        return;
    }
    
    const String& ssid = networks[wui->selectedNetworkIndex_].ssid;
    
    if (ssid.length() == 0) {
        Serial.println("[WiFiUI] Error: Empty SSID");
        return;
    }
    
    Serial.printf("[WiFiUI] Connecting to '%s' with password '%s'\n", 
                  ssid.c_str(), wui->password_.c_str());
    
    // Attempt connection
    wifiManager.connect(ssid.c_str(), wui->password_.c_str());
    
    // Show connection screen
    wui->showConnectionScreen();
    
    // Update status after 2 seconds using lv_timer
    lv_timer_create([](lv_timer_t* timer) {
        WiFiUI* wui = (WiFiUI*)lv_timer_get_user_data(timer);
        if (wui && wui->currentScreen_ == CONNECTION_STATUS) {
            wui->updateConnectionStatus();
        }
        lv_timer_delete(timer); // Run once
    }, 2000, wui);
}

// ============================================================
// Callback: disconnect
// ============================================================
void WiFiUI::disconnectCallback(lv_event_t* e) {
    WiFiUI* wui = (WiFiUI*)lv_event_get_user_data(e);
    Serial.println("[WiFiUI] Disconnecting...");
    wifiManager.disconnect();
    wui->hideConnectionScreen();
    wui->showScreen(NETWORK_LIST);
}

// ============================================================
// Callback: back
// ============================================================
void WiFiUI::backCallback(lv_event_t* e) {
    WiFiUI* wui = (WiFiUI*)lv_event_get_user_data(e);
    if (wui->currentScreen_ == PASSWORD_INPUT) {
        wui->hidePasswordScreen();
        wui->showScreen(NETWORK_LIST);
    } else if (wui->currentScreen_ == CONNECTION_STATUS) {
        wui->hideConnectionScreen();
        wui->showScreen(NETWORK_LIST);
    } else {
        wui->hide();
    }
}

// ============================================================
// Callback: character key pressed
// ============================================================
void WiFiUI::charCallback(lv_event_t* e) {
    WiFiUI* wui = (WiFiUI*)lv_event_get_user_data(e);
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    char keyChar = (char)(intptr_t)lv_obj_get_user_data(obj);
    
    if (keyChar == '\b') { // Backspace
        wui->onDeletePressed();
    } else if (keyChar == '\r') { // Done
        wui->onDonePressed();
    } else if (keyChar == ' ') {
        wui->onSpacePressed();
    } else if (keyChar >= 32 && keyChar <= 126) { // Printable ASCII
        wui->onCharPressed(keyChar);
    }
    
    // Update highlight
    wui->updateKeyboardHighlight(obj);
}

// ============================================================
// Show password screen
// ============================================================
void WiFiUI::showPasswordScreen(int networkIndex) {
    if (networkIndex < 0 || networkIndex >= (int)wifiManager.getNetworks().size()) {
        Serial.println("[WiFiUI] Error: Invalid network index");
        return;
    }
    
    selectedNetworkIndex_ = networkIndex;
    password_ = "";
    passwordVisible_ = false;
    
    const auto& ssid = wifiManager.getNetworks()[networkIndex].ssid;
    
    // Update screen
    lv_obj_t* screen = screens_[PASSWORD_INPUT];
    if (!screen) return;
    
    // Update SSID label in header (child 0)
    lv_obj_t* header = lv_obj_get_child(screen, 0);
    if (header) {
        lv_obj_t* label = lv_obj_get_child(header, 0);
        if (label) {
            char title[64];
            snprintf(title, sizeof(title), "Connect to: %s", ssid.c_str());
            lv_label_set_text(label, title);
        }
    }
    
    // Update password display
    updatePasswordDisplay();
    
    // Show screen
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_HIDDEN);
    currentScreen_ = PASSWORD_INPUT;
    
    // Show keyboard
    if (keyboard_container_) {
        lv_obj_clear_flag(keyboard_container_, LV_OBJ_FLAG_HIDDEN);
    }
    
    Serial.printf("[WiFiUI] Showing password screen for '%s'\n", ssid.c_str());
}

// ============================================================
// Hide password screen
// ============================================================
void WiFiUI::hidePasswordScreen() {
    if (screens_[PASSWORD_INPUT]) {
        lv_obj_add_flag(screens_[PASSWORD_INPUT], LV_OBJ_FLAG_HIDDEN);
    }
    if (keyboard_container_) {
        lv_obj_add_flag(keyboard_container_, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================================
// Show connection screen
// ============================================================
void WiFiUI::showConnectionScreen() {
    if (!screens_[CONNECTION_STATUS]) {
        createConnectionStatusScreen();
    }
    
    lv_obj_t* screen = screens_[CONNECTION_STATUS];
    if (!screen) return;
    
    // Update status label (child 2)
    lv_obj_t* statusLbl = lv_obj_get_child(screen, 2);
    if (statusLbl) {
        lv_label_set_text(statusLbl, "[..] Connecting...");
    }
    
    // Show screen
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_HIDDEN);
    currentScreen_ = CONNECTION_STATUS;
    
    // Hide keyboard
    if (keyboard_container_) {
        lv_obj_add_flag(keyboard_container_, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================================
// Hide connection screen
// ============================================================
void WiFiUI::hideConnectionScreen() {
    if (screens_[CONNECTION_STATUS]) {
        lv_obj_add_flag(screens_[CONNECTION_STATUS], LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================================
// Keyboard handlers
// ============================================================
void WiFiUI::onCharPressed(char ch) {
    password_ += ch;
    updatePasswordDisplay();
}

void WiFiUI::onDeletePressed() {
    if (!password_.empty()) {
        password_.pop_back();
        updatePasswordDisplay();
    }
}

void WiFiUI::onSpacePressed() {
    password_ += ' ';
    updatePasswordDisplay();
}

void WiFiUI::onDonePressed() {
    // Auto-connect when done is pressed
    Serial.printf("[WiFiUI] Done pressed, connecting with password length %zu\n", password_.length());
    
    // Find connect button and call connectCallback directly
    connectCallback(nullptr);
}

void WiFiUI::updatePasswordDisplay() {
    if (password_display_) {
        if (passwordVisible_) {
            lv_label_set_text_fmt(password_display_, "Password: %s", password_.c_str());
        } else {
            std::string masked = "Password: ";
            for (size_t i = 0; i < password_.size(); i++) {
                masked += BULLET;
            }
            if (masked.length() <= 10) {
                masked = "Password: ............";
            }
            lv_label_set_text(password_display_, masked.c_str());
        }
    }
}

void WiFiUI::updateKeyboardHighlight(lv_obj_t* target) {
    // Update visual highlight on keyboard
    if (currentHighlight_) {
        lv_obj_set_style_bg_color(currentHighlight_, BTN_COLOR, 0);
    }
    currentHighlight_ = target;
    if (currentHighlight_) {
        lv_obj_set_style_bg_color(currentHighlight_, ACCENT_COLOR, 0);
    }
}
