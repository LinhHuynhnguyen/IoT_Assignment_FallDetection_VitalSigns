#include "global.h"
#include "sensor_vitals.h"
#include "fall_detection.h"
#include "espnow_sender.h"
#include "Wire.h"

void setup() {
    Serial.begin(115200);
    xDataMutex = xSemaphoreCreateMutex(); // Tạo Mutex trước khi tạo Task
    xI2CMutex  = xSemaphoreCreateMutex();
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

#if !USE_FAKE_DATA
    xTaskCreate(task_read_vitals,    "Vitals", 4096, NULL, 2, NULL);
    xTaskCreate(task_fall_detection, "Fall",   8192, NULL, 3, NULL); // mẫu 50 Hz nên ưu tiên cao hơn
#endif
    xTaskCreate(task_espnow_send,    "ESPNow", 4096, NULL, 2, &xEspNowTaskHandle);
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
