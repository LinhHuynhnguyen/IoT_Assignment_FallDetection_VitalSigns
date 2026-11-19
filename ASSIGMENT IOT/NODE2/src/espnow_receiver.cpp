// src/espnow_receiver.cpp
#include "espnow_receiver.h"
#include <esp_now.h>

typedef struct __attribute__((packed)) {
    float heart_rate;
    float spo2;
    bool is_fall;
    float fall_confidence;
} SensorPacket;

// Callback tự động chạy khi có dữ liệu từ Node 1 bay tới
// Callback tự động chạy khi có dữ liệu từ Node 1 bay tới (Chuẩn ESP32 Core 3.x)
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingData, int len) {
    if (len == sizeof(SensorPacket)) {
        SensorPacket pkt;
        memcpy(&pkt, incomingData, sizeof(pkt));
        glob_hr = pkt.heart_rate;
        glob_spo2 = pkt.spo2;
        glob_is_fall = pkt.is_fall;
        glob_last_seen_ms = millis(); // Cập nhật thời điểm nhận tin cuối
    }
}

void init_espnow_receiver() {
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(OnDataRecv);
    }
}