#pragma once

#include <lvgl.h>
#include <vector>
#include <string>

class WiFiUI {
public:
    /**
     * Initialize the WiFi UI overlay.
     * Call this once after the main UI is created.
     * @param parent The parent container/screen object to attach WiFi screens to
     */
    void init(lv_obj_t* parent);
    
    /** Show the WiFi network list screen */
    void show();
    
    /** Hide all WiFi screens */
    void hide();
    
    /** Refresh the network list (called after a scan completes) */
    void refreshNetworkList();
    
    /** Update the connection status screen */
    void updateConnectionStatus();
    
    /** Handle periodic tasks (scan completion, UI updates). Call from loop() */
    void handle();

private:
    enum Screen { NETWORK_LIST, PASSWORD_INPUT, CONNECTION_STATUS };
    
    void createNetworkListScreen();
    void createPasswordInputScreen();
    void createConnectionStatusScreen();
    void createOnScreenKeyboard();
    
    void showScreen(Screen screen);
    
    static void networkSelectCallback(lv_event_t* e);
    static void scanCallback(lv_event_t* e);
    static void connectCallback(lv_event_t* e);
    static void disconnectCallback(lv_event_t* e);
    static void backCallback(lv_event_t* e);
    static void charCallback(lv_event_t* e);
    
    void showPasswordScreen(int networkIndex);
    void hidePasswordScreen();
    void showConnectionScreen();
    void hideConnectionScreen();
    
    // On-screen keyboard helpers
    void onCharPressed(char ch);
    void onDeletePressed();
    void onSpacePressed();
    void onDonePressed();
    void updatePasswordDisplay();
    void updateKeyboardHighlight(lv_obj_t* target);
    
    // UI helpers
    lv_obj_t* createStyledButton(const char* text, uint32_t bgColor, uint32_t textColor, lv_obj_t* parent, lv_coord_t col, lv_coord_t col_span);
    lv_obj_t* createNetworkItem(const char* ssid, int rssi, uint8_t encryption, lv_obj_t* parent, int index);
    const char* getEncryptionIcon(uint8_t encryption);
    const char* getSignalBars(int rssi);
    
    lv_obj_t* parent_;
    lv_obj_t* screens_[3];
    Screen currentScreen_;
    
    // Password input state
    int selectedNetworkIndex_;
    std::string password_;
    bool passwordVisible_;
    
    // Keyboard state
    lv_obj_t* keyboard_container_;
    lv_obj_t* password_display_;
    lv_obj_t* currentHighlight_;
    
    // Network list items (for hiding/showing)
    std::vector<lv_obj_t*> network_items_;
};

// Global instance
extern WiFiUI wifiUI;
