#include "sensor_vitals.h"
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 particleSensor;

const byte RATE_SIZE = 4; // Tăng giá trị này để lấy trung bình mượt hơn (4 là mức chuẩn)
byte rates[RATE_SIZE];    // Mảng lưu các giá trị nhịp tim
byte rateSpot = 0;
long lastBeat = 0;        // Thời điểm xảy ra nhịp đập gần nhất

float beatsPerMinute = 0;
int beatAvg = 0;

void task_read_vitals(void *pvParameters) {
    // Khởi tạo bus I2C chuẩn của YoloUNO (SDA = 11, SCL = 12)
    // Wire.begin(11, 12);

    Serial.println("[MAX30102] Initializing...");

    // Khởi tạo cảm biến (dùng vTaskDelete thay vì while(1) để không treo các task khác)
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("[MAX30102] Was not found. Please check wiring/power.");
        vTaskDelete(NULL);
    }

    Serial.println("[MAX30102] Place your index finger on the sensor with steady pressure.");

    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x0A);   // Bật LED đỏ mức thấp để báo cảm biến đang chạy
    particleSensor.setPulseAmplitudeGreen(0);    // Tắt LED xanh lá

    unsigned long lastPrintMs = 0;

    // Vòng lặp chính của Task FreeRTOS
    while (1) {
        long irValue  = particleSensor.getIR();
        long redValue = particleSensor.getRed();

        // Kiểm tra phát hiện nhịp đập
        if (checkForBeat(irValue) == true) {
            long delta = millis() - lastBeat; // Đã bổ sung dấu trừ (-) bị thiếu
            lastBeat = millis();

            beatsPerMinute = 60 / (delta / 1000.0);

            if (beatsPerMinute < 255 && beatsPerMinute > 20) {
                rates[rateSpot++] = (byte)beatsPerMinute; // Lưu vào mảng
                rateSpot %= RATE_SIZE;                    // Xoay vòng chỉ số mảng

                // Tính trung bình cộng nhịp tim
                beatAvg = 0;
                for (byte x = 0; x < RATE_SIZE; x++) {
                    beatAvg += rates[x];
                }
                beatAvg /= RATE_SIZE;
            }
        }

        // Xử lý trạng thái đặt ngón tay & tính SpO2 đi kèm
        float current_hr   = 0.0;
        float current_spo2 = 0.0;

        if (irValue < 50000) {
            // Không phát hiện ngón tay -> reset chỉ số về 0
            beatAvg = 0;
            beatsPerMinute = 0;
            current_hr = 0.0;
            current_spo2 = 0.0;
        } else {
            current_hr = (float)beatAvg;
            // Ước lượng SpO2 dựa trên tỷ lệ hấp thụ ánh sáng Red/IR (giới hạn 90 - 99%)
            if (irValue > 0 && redValue > 0) {
                float ratio = (float)redValue / (float)irValue;
                current_spo2 = constrain(110.0 - 15.0 * ratio, 90.0, 99.5);
            }
        }

        // Cập nhật kết quả vào biến toàn cục (có khóa Mutex bảo vệ cho ESP-NOW)
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            glob_data.heart_rate = current_hr;
            glob_data.spo2       = current_spo2;
            xSemaphoreGive(xDataMutex);
        }

        if (millis() - lastPrintMs >= 500) {
            lastPrintMs = millis();
            Serial.print("IR=");
            Serial.print(irValue);
            Serial.print(", BPM=");
            Serial.print(beatsPerMinute);
            Serial.print(", Avg BPM=");
            Serial.print(beatAvg);
            Serial.print(", SpO2=");
            Serial.print(current_spo2);
            if (irValue < 50000) {
                Serial.print(" No finger?");
            }
            Serial.println();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// #include "sensor_vitals.h"

// void task_read_vitals(void *pvParameters) {
//     while (1) {
//         // Giả lập nhịp tim 70 - 85 bpm, SpO2 96 - 99% (tương tự LAB 4)
//         float hr   = 70.0 + random(0, 1500) / 100.0;
//         float spo2 = 96.0 + random(0, 300) / 100.0;

//         if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
//             glob_data.heart_rate = hr;
//             glob_data.spo2 = spo2;
//             xSemaphoreGive(xDataMutex);
//         }
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }