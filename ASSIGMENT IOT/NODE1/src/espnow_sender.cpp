// src/espnow_sender.cpp
#include "espnow_sender.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <WiFi.h>

static volatile uint32_t sent_ok = 0;
static volatile uint32_t sent_fail = 0;

// Chữ ký callback gửi đổi từ ESP-IDF 5.5 (Arduino core 3.3+)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
#else
static void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
#endif
    if (status == ESP_NOW_SEND_SUCCESS) sent_ok++; else sent_fail++;
}

static bool isBroadcast(const uint8_t *mac) {
    for (int i = 0; i < 6; i++) if (mac[i] != 0xFF) return false;
    return true;
}

void task_espnow_send(void *pvParameters) {
    WiFi.mode(WIFI_STA); // Bắt buộc bật chế độ STA để dùng ESP-NOW
    WiFi.disconnect();
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE); // phải trùng kênh Node 2

    Serial.print("[ESPNOW] MAC Node 1: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] Error initializing ESP-NOW");
        vTaskDelete(NULL);
    }
    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, node2_mac_addr, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ESPNOW] Failed to add peer");
        vTaskDelete(NULL);
    }
    if (isBroadcast(node2_mac_addr)) {
        Serial.println("[ESPNOW] Dang gui BROADCAST (chua dat MAC Node 2 trong config.h)");
    }

    uint32_t seq = 0;
    SensorPacket pkt;
    uint8_t last_fall = 0;

    while (1) {
        // Lấy snapshot dữ liệu cảm biến; nếu không lấy được mutex thì bỏ qua lượt này
        bool got = false;
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            pkt = glob_data;
            xSemaphoreGive(xDataMutex);
            got = true;
        }

#if USE_FAKE_DATA
        // Dữ liệu giả để B test nhận khi chưa có cảm biến thật
        pkt.hr    = 70 + random(0, 15);
        pkt.spo2  = 96 + random(0, 4);
        pkt.fall  = 0;
        pkt.valid = 1;
        got = true;
#endif

        if (got) {
            pkt.seq = seq++;
            esp_err_t r = esp_now_send(node2_mac_addr, (uint8_t *)&pkt, sizeof(pkt));
            Serial.printf("[ESPNOW] TX seq=%lu hr=%u spo2=%u fall=%u valid=%u -> %s (ok=%lu fail=%lu)\n",
                          (unsigned long)pkt.seq, pkt.hr, pkt.spo2, pkt.fall, pkt.valid,
                          r == ESP_OK ? "queued" : "ERR",
                          (unsigned long)sent_ok, (unsigned long)sent_fail);
            last_fall = pkt.fall;
        }

        // Ngủ theo chu kỳ, nhưng thoát sớm nếu task phát hiện ngã gọi xTaskNotifyGive
        // (sự kiện ngã gửi ngay). Khi đang ngã thì gửi dồn mỗi 200 ms.
        uint32_t period = last_fall ? 200 : ESPNOW_SEND_PERIOD_MS;
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(period));
    }
}
