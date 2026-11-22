// src/espnow_receiver.cpp
#include "espnow_receiver.h"
#include <esp_err.h>
#include <esp_now.h>
#include <freertos/queue.h>

typedef struct __attribute__((packed)) {
    uint8_t hr;
    uint8_t spo2;
    uint8_t fall;
    uint8_t valid;
    uint32_t seq;
} SensorPacket;
static_assert(sizeof(SensorPacket) == 8, "ESP-NOW sensor packet must be 8 bytes");

typedef struct {
    SensorPacket packet;
    uint32_t lost;
} RxLogEntry;

static QueueHandle_t rx_log_queue = nullptr;
static bool has_last_seq = false;
static uint32_t last_seq = 0;
static uint32_t lost_packets = 0;

static bool checkEspNowSeq(uint32_t seq, unsigned long now_ms) {
    constexpr unsigned long seq_reset_timeout_ms = 5000;
    if (!has_last_seq || now_ms - glob_last_seen_ms >= seq_reset_timeout_ms) {
        has_last_seq = true;
        last_seq = seq;
        return true;
    }

    // Phép trừ không dấu vẫn đúng khi seq quay từ UINT32_MAX về 0.
    const uint32_t delta = seq - last_seq;
    if (delta == 0 || delta >= 0x80000000U) {
        return false;
    }

    lost_packets += delta - 1;
    last_seq = seq;
    return true;
}

// Callback nhận gói theo API của framework ESP32 đang dùng.
static void OnDataRecv(const uint8_t * /*mac*/, const uint8_t *incomingData, int len) {
    if (len == sizeof(SensorPacket)) {
        SensorPacket pkt;
        memcpy(&pkt, incomingData, sizeof(pkt));
        const unsigned long now_ms = millis();
        if (!checkEspNowSeq(pkt.seq, now_ms)) {
            return;
        }

        glob_hr = pkt.hr;
        glob_spo2 = pkt.spo2;
        glob_is_fall = pkt.fall;
        glob_sensor_valid = pkt.valid;
        if (pkt.valid == 0) {
            glob_hr = 0;
            glob_spo2 = 0;
        }
        glob_last_seen_ms = now_ms; // Cập nhật thời điểm nhận tin cuối
        if (rx_log_queue != nullptr) {
            const RxLogEntry entry{pkt, lost_packets};
            xQueueSend(rx_log_queue, &entry, 0);
        }
    }
}

void init_espnow_receiver() {
    const esp_err_t initResult = esp_now_init();
    if (initResult != ESP_OK) {
        Serial.printf("[ESP-NOW] Init failed: %s (%d)\n", esp_err_to_name(initResult), initResult);
        return;
    }

    rx_log_queue = xQueueCreate(8, sizeof(RxLogEntry));
    if (rx_log_queue == nullptr) {
        Serial.println("[ESP-NOW] Create queue failed");
    }

    const esp_err_t registerResult = esp_now_register_recv_cb(OnDataRecv);
    if (registerResult != ESP_OK) {
        Serial.printf("[ESP-NOW] Register callback failed: %s (%d)\n", esp_err_to_name(registerResult), registerResult);
        esp_now_deinit();
        if (rx_log_queue != nullptr) {
            vQueueDelete(rx_log_queue);
            rx_log_queue = nullptr;
        }
        return;
    }

    Serial.println("[ESP-NOW] RX ready");
}

void logEspNowRx() {
    if (rx_log_queue == nullptr) {
        return;
    }

    RxLogEntry entry;
    while (xQueueReceive(rx_log_queue, &entry, 0) == pdTRUE) {
        Serial.printf("[ESP-NOW RX] seq=%lu lost=%lu hr=%u spo2=%u fall=%u valid=%u\n",
                      static_cast<unsigned long>(entry.packet.seq),
                      static_cast<unsigned long>(entry.lost),
                      static_cast<unsigned int>(entry.packet.hr),
                      static_cast<unsigned int>(entry.packet.spo2),
                      static_cast<unsigned int>(entry.packet.fall),
                      static_cast<unsigned int>(entry.packet.valid));
    }
}
