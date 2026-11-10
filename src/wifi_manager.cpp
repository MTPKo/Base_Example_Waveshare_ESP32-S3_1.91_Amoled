#include "wifi_manager.h"
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <Arduino.h>

// Global instance
WiFiManager wifiManager;

// Static event handler wrapper - needs to be static because esp_event_handler_instance_register
// requires a function pointer with C linkage, not a member function pointer
static void IRAM_ATTR wifi_event_wrapper(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    WiFiManager* mgr = static_cast<WiFiManager*>(arg);
    if (mgr) {
        mgr->eventHandler(event_base, event_id, event_data);
    }
}

void WiFiManager::begin() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("WiFiManager", "NVS partition corrupted, erasing...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE("WiFiManager", "NVS flash init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Initialize network stack
    esp_netif_init();
    
    // Create default event loop
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE("WiFiManager", "Event loop creation failed: %s", esp_err_to_name(ret));
        return;
    }

    // Set WiFi mode to STA
    WiFi.mode(WIFI_STA);
    
    // Set auto-reconnect
    WiFi.setAutoReconnect(true);
    
    // Register event handlers using static wrapper
    esp_event_handler_instance_t instanceId = nullptr;
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_wrapper, this, &instanceId);
    if (ret != ESP_OK) {
        ESP_LOGE("WiFiManager", "WIFI_EVENT handler registration failed: %s", esp_err_to_name(ret));
    }
    
    ret = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, wifi_event_wrapper, this, &instanceId);
    if (ret != ESP_OK) {
        ESP_LOGE("WiFiManager", "IP_EVENT handler registration failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI("WiFiManager", "WiFi Manager initialized");
}

int WiFiManager::scan() {
    if (scanning_) {
        return -1;
    }
    
    if (!connected_ && WiFi.status() != WL_CONNECTED) {
        scanning_ = true;
        networks_.clear();
        
        int status = WiFi.scanNetworks(true, false);
        if (status == WIFI_SCAN_RUNNING) {
            Serial.println("[WiFiManager] Scan started");
            return -1;
        } else {
            scanning_ = false;
            ESP_LOGE("WiFiManager", "Scan failed: %d", status);
            return status;
        }
    } else {
        Serial.println("[WiFiManager] Cannot scan while connected");
        return -1;
    }
}

bool WiFiManager::connect(const char* ssid, const char* password) {
    // Store credentials in NVS
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_set_str(nvs_handle, "ssid", ssid);
        nvs_set_str(nvs_handle, "pass", password);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        Serial.printf("[WiFiManager] Stored credentials for '%s'\n", ssid);
    } else {
        ESP_LOGE("WiFiManager", "Failed to open NVS: %s", esp_err_to_name(ret));
    }
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    connected_ = true;
    currentSSID_ = ssid;
    
    Serial.printf("[WiFiManager] Connecting to '%s'\n", ssid);
    return true;
}

void WiFiManager::disconnect() {
    WiFi.disconnect(true);  // erase AP credentials
    connected_ = false;
    currentSSID_ = "";
    currentIP_ = "";
    currentRssi_ = 0;
    
    // Clear stored credentials from NVS
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_erase_key(nvs_handle, "ssid");
        nvs_erase_key(nvs_handle, "pass");
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        Serial.println("[WiFiManager] Cleared stored credentials");
    }
}

String WiFiManager::getMAC() const {
    return WiFi.macAddress();
}

String WiFiManager::getGatewayIP() const {
    return WiFi.gatewayIP().toString();
}

String WiFiManager::getSubnetMask() const {
    return WiFi.subnetMask().toString();
}

String WiFiManager::getDNS() const {
    return WiFi.dnsIP().toString();
}

// This is the actual event handler called from the static wrapper
void WiFiManager::eventHandler(esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_CONNECTED: {
                Serial.println("[WiFiManager] Connected to AP");
                connected_ = true;
                // Get AP info
                wifi_event_sta_connected_t* connected_event = (wifi_event_sta_connected_t*)event_data;
                char ssid_buf[33];
                memset(ssid_buf, 0, sizeof(ssid_buf));
                memcpy(ssid_buf, connected_event->ssid, connected_event->ssid_len);
                currentSSID_ = String(ssid_buf);
                Serial.printf("[WiFiManager] SSID: %s\n", currentSSID_.c_str());
                break;
            }
                
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*)event_data;
                Serial.printf("[WiFiManager] Disconnected. Reason: %d\n", disconnected->reason);
                connected_ = false;
                currentSSID_ = "";
                currentIP_ = "";
                currentRssi_ = 0;
                break;
            }
                
            case WIFI_EVENT_SCAN_DONE: {
                wifi_event_sta_scan_done_t* scanDone = (wifi_event_sta_scan_done_t*)event_data;
                if (scanDone->status == 0) { // status == 0 means success
                    int num = scanDone->number;
                    Serial.printf("[WiFiManager] Scan completed: %d networks found\n", num);
                    
                    for (int i = 0; i < num; i++) {
                        Network net;
                        net.ssid = WiFi.SSID(i);
                        net.rssi = WiFi.RSSI(i);
                        net.encryption = WiFi.encryptionType(i);
                        net.isHidden = false; // WiFi.scanNetworks doesn't provide hidden status directly
                        networks_.push_back(net);
                        
                        Serial.printf("[WiFiManager] %d: %s (%d dBm) %s\n", 
                            i + 1, net.ssid.c_str(), net.rssi,
                            net.encryption == WIFI_AUTH_OPEN ? "OPEN" : "SECURED");
                    }
                    
                    // Free scan results
                    WiFi.scanDelete();
                } else {
                    Serial.println("[WiFiManager] Scan completed with errors");
                    WiFi.scanDelete();
                }
                scanning_ = false;
                break;
            }
                
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                currentIP_ = WiFi.localIP().toString();
                Serial.printf("[WiFiManager] Got IP: %s\n", currentIP_.c_str());
                Serial.printf("[WiFiManager] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
                Serial.printf("[WiFiManager] Subnet: %s\n", WiFi.subnetMask().toString().c_str());
                Serial.printf("[WiFiManager] DNS: %s\n", WiFi.dnsIP().toString().c_str());
                break;
            }
                
            default:
                break;
        }
    }
}
