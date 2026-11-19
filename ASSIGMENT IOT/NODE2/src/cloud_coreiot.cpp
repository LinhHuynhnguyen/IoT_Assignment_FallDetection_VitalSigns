#include "global.h"
#include "cloud_coreiot.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPUpdate.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char* COREIOT_SERVER = "app.coreiot.io";
const char* ACCESS_TOKEN   = "YenLinh2162006";

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
    Serial.println("[CoreIoT RX]: " + msg);

    JsonDocument doc;
    if (deserializeJson(doc, msg)) return;

    String topicStr = String(topic);

    // 1. Xử lý lệnh điều khiển RPC từ Dashboard (Chỉ giữ Buzzer và chỉnh ngưỡng)
    if (topicStr.startsWith("v1/devices/me/rpc/request/")) {
        String method = doc["method"] | "";
        if (method == "setBuzzer") {
            bool cmd = doc["params"];
            glob_buzzer_state = cmd;
            // Nếu người dùng gạt OFF thì tắt còi luôn và khóa không cho tự hú lại ngay
            glob_buzzer_muted = !cmd; 
        } else if (method == "setHrThreshold") {
            glob_hr_max_threshold = doc["params"];
        }
    }

    // 2. Xử lý cập nhật Firmware OTA từ Shared Attributes của CoreIoT
    if (topicStr == "v1/devices/me/attributes") {
        if (doc["fw_url"].is<const char*>()) {
            String fw_url = doc["fw_url"].as<String>();
            Serial.println("[OTA] Starting update from: " + fw_url);
            WiFiClient otaClient;
            t_httpUpdate_return ret = httpUpdate.update(otaClient, fw_url);
            if (ret == HTTP_UPDATE_FAILED) {
                Serial.printf("[OTA] Failed (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            }
        }
    }
}

void task_cloud_coreiot(void *pvParameters) {
    xSemaphoreTake(xBinarySemaphoreInternet, portMAX_DELAY);

    mqttClient.setServer(COREIOT_SERVER, 1883);
    mqttClient.setCallback(mqttCallback);

    while (1) {
        if (isWifiConnected) {
            if (!mqttClient.connected()) {
                if (mqttClient.connect("Node2_Gateway", ACCESS_TOKEN, NULL)) {
                    Serial.println("[CoreIoT] Connected!");
                    mqttClient.subscribe("v1/devices/me/rpc/request/+");
                    mqttClient.subscribe("v1/devices/me/attributes");
                }
            }
            mqttClient.loop();

            // Gửi Telemetry mỗi 2 giây
            JsonDocument outDoc;
            outDoc["heart_rate"]   = glob_hr;
            outDoc["spo2"]         = glob_spo2;
            outDoc["is_fall"]      = glob_is_fall;
            outDoc["buzzer"]       = glob_buzzer_state;
            outDoc["node1_online"] = (millis() - glob_last_seen_ms < 5000);

            char buffer[256];
            serializeJson(outDoc, buffer);
            mqttClient.publish("v1/devices/me/telemetry", buffer);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}