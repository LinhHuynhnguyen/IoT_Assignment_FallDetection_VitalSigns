// src/espnow_sender.cpp
#include "espnow_sender.h"
#include <esp_now.h>
#include <WiFi.h>

void task_espnow_send(void *pvParameters) {
    WiFi.mode(WIFI_STA); // Bắt buộc bật chế độ STA để dùng ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        vTaskDelete(NULL);
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, node2_mac_addr, 6);
    peerInfo.channel = 0; // Lưu ý: Phải trùng kênh Wi-Fi của Node 2!
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    SensorPacket packetToSend;

    while (1) {
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            packetToSend = glob_data;
            xSemaphoreGive(xDataMutex);
        }
        esp_now_send(node2_mac_addr, (uint8_t *)&packetToSend, sizeof(packetToSend));
        
        // Nếu phát hiện té ngã, gửi ngay lập tức (200ms/lần), bình thường gửi 1s/lần
        vTaskDelay(pdMS_TO_TICKS(packetToSend.is_fall ? 200 : 1000));
    }
}