#include "fall_detection.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

Adafruit_MPU6050 mpu;

// Nút BOOT (GPIO 0) làm nút hủy báo động giả
#ifndef CANCEL_BUTTON_PIN
#define CANCEL_BUTTON_PIN 0
#endif

// Các pha của thuật toán phát hiện ngã
enum FallStage { STAGE_IDLE = 0, STAGE_FREEFALL, STAGE_IMPACT, STAGE_TILT };

static void publishFall(uint8_t fall) {
    if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        glob_data.fall = fall;
        xSemaphoreGive(xDataMutex);
    }
    // Đánh thức task ESP-NOW để gửi ngay, không chờ hết chu kỳ
    if (xEspNowTaskHandle) xTaskNotifyGive(xEspNowTaskHandle);
}

void task_fall_detection(void *pvParameters) {
    pinMode(LED_FALL_PIN, OUTPUT);
    pinMode(CANCEL_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_FALL_PIN, LOW);

    Serial.println("[MPU6050] Initializing...");
    bool ok = false;
    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        ok = mpu.begin();
        if (ok) {
            mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
            mpu.setGyroRange(MPU6050_RANGE_500_DEG);
            mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        }
        xSemaphoreGive(xI2CMutex);
    }
    if (!ok) {
        Serial.println("[MPU6050] Failed to find MPU6050 chip! Task stopped.");
        vTaskDelete(NULL);
    }
    Serial.println("[MPU6050] Found!");
    vTaskDelay(pdMS_TO_TICKS(100));

    FallStage stage = STAGE_IDLE;
    unsigned long stageStart = 0;
    bool pending_fall = false;        // đang trong thời gian ân hạn chờ bấm nút hủy
    unsigned long pending_start = 0;
    bool confirmed_fall = false;
    unsigned long lastPrintMs = 0;

    // Lấy mẫu đều 50 Hz (20 ms)
    TickType_t xLastWake = xTaskGetTickCount();

    while (1) {
        sensors_event_t a, g, temp;
        bool got = false;
        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            got = mpu.getEvent(&a, &g, &temp);
            xSemaphoreGive(xI2CMutex);
        }
        if (!got) {
            vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(MPU_SAMPLE_PERIOD_MS));
            continue;
        }

        unsigned long now = millis();

        // m/s^2 -> g
        float ax_g = a.acceleration.x / 9.80665f;
        float ay_g = a.acceleration.y / 9.80665f;
        float az_g = a.acceleration.z / 9.80665f;
        float svm = sqrtf(ax_g * ax_g + ay_g * ay_g + az_g * az_g);

        // rad/s -> deg/s
        float gx = g.gyro.x * 57.29578f;
        float gy = g.gyro.y * 57.29578f;
        float gz = g.gyro.z * 57.29578f;
        float gyro_mag = sqrtf(gx * gx + gy * gy + gz * gz);

#if SERIAL_OUTPUT_MODE == 1
        // Dạng CSV để vẽ đồ thị SVM: ms,ax,ay,az,svm (đơn vị g)
        Serial.printf("%lu,%.3f,%.3f,%.3f,%.3f\n", now, ax_g, ay_g, az_g, svm);
#endif

        // --- THUẬT TOÁN 3 PHA: rơi tự do -> va đập -> đổi tư thế -> bất động ---
        if (!pending_fall && !confirmed_fall) {
            switch (stage) {
            case STAGE_IDLE:
                if (svm <= FREEFALL_G_THRESH) {
                    stage = STAGE_FREEFALL;
                    stageStart = now;
                    Serial.println("[FALL] Trigger 1: roi tu do");
                }
                break;
            case STAGE_FREEFALL:
                if (svm >= IMPACT_G_THRESH) {
                    stage = STAGE_IMPACT;
                    stageStart = now;
                    Serial.printf("[FALL] Trigger 2: va dap %.2f g\n", svm);
                } else if (svm <= FREEFALL_G_THRESH) {
                    stageStart = now; // còn đang rơi tự do -> gia hạn cửa sổ
                } else if (now - stageStart >= PHASE_WINDOW_MS) {
                    stage = STAGE_IDLE;
                }
                break;
            case STAGE_IMPACT:
                if (gyro_mag >= GYRO_CHANGE_THRESH) {
                    stage = STAGE_TILT;
                    stageStart = now;
                    Serial.println("[FALL] Trigger 3: doi tu the, kiem tra bat dong...");
                } else if (now - stageStart >= PHASE_WINDOW_MS) {
                    stage = STAGE_IDLE;
                }
                break;
            case STAGE_TILT:
                if (now - stageStart >= STILL_CHECK_MS) {
                    if (gyro_mag <= STILL_GYRO_THRESH) {
                        pending_fall = true;
                        pending_start = now;
                        Serial.println("[FALL] NGHI NGO TE NGA! Bam nut BOOT trong 5s de huy neu bao gia");
                    } else {
                        Serial.println("[FALL] Nguoi dung tu dung day -> huy canh bao");
                    }
                    stage = STAGE_IDLE;
                }
                break;
            }
        }

        // --- THỜI GIAN ÂN HẠN & NÚT HỦY ---
        if (pending_fall) {
            digitalWrite(LED_FALL_PIN, (now / 200) % 2);

            if (digitalRead(CANCEL_BUTTON_PIN) == LOW) {
                pending_fall = false;
                digitalWrite(LED_FALL_PIN, LOW);
                Serial.println("[FALL] DA HUY BAO DONG GIA BANG NUT NHAN");
                vTaskDelay(pdMS_TO_TICKS(300)); // chống dội phím
                xLastWake = xTaskGetTickCount();
            } else if (now - pending_start >= GRACE_PERIOD_MS) {
                pending_fall = false;
                confirmed_fall = true;
                digitalWrite(LED_FALL_PIN, HIGH);
                Serial.println("[FALL] XAC NHAN TE NGA KHAN CAP! Gui ESP-NOW ngay");
                publishFall(1); // gửi ngay lập tức
            }
        }

        // Bấm BOOT để reset trạng thái sau khi đã xử lý sự cố
        if (confirmed_fall && digitalRead(CANCEL_BUTTON_PIN) == LOW) {
            confirmed_fall = false;
            digitalWrite(LED_FALL_PIN, LOW);
            Serial.println("[FALL] Da xac nhan an toan, reset trang thai");
            publishFall(0);
            vTaskDelay(pdMS_TO_TICKS(300));
            xLastWake = xTaskGetTickCount();
        }

#if SERIAL_OUTPUT_MODE == 0
        if (now - lastPrintMs >= 500) {
            lastPrintMs = now;
            Serial.printf("[MPU6050] SVM:%.2fg | Gyro:%.1f deg/s | State: %s\n",
                          svm, gyro_mag,
                          confirmed_fall ? "FALL_CONFIRMED" : (pending_fall ? "PENDING" : "NORMAL"));
        }
#endif

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(MPU_SAMPLE_PERIOD_MS));
    }
}
