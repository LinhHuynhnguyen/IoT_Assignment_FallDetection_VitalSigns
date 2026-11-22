#include "global.h"
#include "espnow_receiver.h"
#include "local_alert.h"
#include "web_server.h"
#include "cloud_coreiot.h"
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    
    // 1. Tạo Semaphore trước khi tạo Task (Bắt buộc như LAB 4)
    xBinarySemaphoreInternet = xSemaphoreCreateBinary();

    // 2. Bật chế độ AP_STA và khởi động ESP-NOW Receiver
    WiFi.mode(WIFI_AP_STA);
    Serial.print("Node 2 STA MAC: ");
    Serial.println(WiFi.macAddress());
    init_espnow_receiver();

    // 3. Khởi tạo các Task FreeRTOS
    xTaskCreate(main_server_task,   "WebServer", 8192, NULL, 2, NULL);
    xTaskCreate(task_local_alert,   "OLED_Buzz", 4096, NULL, 2, NULL);
    xTaskCreate(task_cloud_coreiot, "CoreIoT",   8192, NULL, 1, NULL); // Stack 8KB cho MQTT + OTA
}

void loop() {
    logEspNowRx();
    vTaskDelay(pdMS_TO_TICKS(1000));
}
