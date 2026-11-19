#include "global.h"
#include "sensor_vitals.h"
#include "fall_detection.h"
#include "espnow_sender.h"
#include "Wire.h"

void setup() {
    Serial.begin(115200);
    xDataMutex = xSemaphoreCreateMutex(); // Tạo Mutex trước khi tạo Task
    Wire.begin(11, 12);

    xTaskCreate(task_read_vitals,    "Vitals", 4096, NULL, 1, NULL);
    xTaskCreate(task_fall_detection, "TinyML", 8192, NULL, 2, NULL); // Stack lớn cho AI
    xTaskCreate(task_espnow_send,    "ESPNow", 4096, NULL, 2, NULL);
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }