#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <esp_event.h>

class WiFiManager {
public:
    void begin();
    int scan();                          // Scan for networks, returns count (-1 if scanning)
    bool connect(const char* ssid, const char* password);
    void disconnect();
    
    bool isConnected() const { return connected_; }
    int8_t getRssi() const { return currentRssi_; }
    String getSSID() const { return currentSSID_; }
    String getIP() const { return currentIP_; }
    String getMAC() const;
    String getGatewayIP() const;
    String getSubnetMask() const;
    String getDNS() const;
    
    struct Network {
        String ssid;
        int rssi;
        uint8_t encryption;              // WIFI_AUTH_OPEN, WIFI_AUTH_WPA2_PSK, etc.
        bool isHidden;
    };
    
    const std::vector<Network>& getNetworks() const { return networks_; }
    bool isScanning() const { return scanning_; }

public:
    void eventHandler(esp_event_base_t event_base, int32_t event_id, void* event_data);
    
    std::vector<Network> networks_;
    bool connected_ = false;
    bool scanning_ = false;
    int8_t currentRssi_ = 0;
    String currentSSID_;
    String currentIP_;
    esp_event_handler_instance_t eventHandlerInstance_ = nullptr;
};

// Global instance
extern WiFiManager wifiManager;
