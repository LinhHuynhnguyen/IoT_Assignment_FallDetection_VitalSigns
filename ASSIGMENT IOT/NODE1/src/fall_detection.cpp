#include "fall_detection.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

Adafruit_MPU6050 mpu;

// Sử dụng nút BOOT có sẵn trên mạch YoloUNO (GPIO 0) làm nút hủy báo động giả
#ifndef CANCEL_BUTTON_PIN
#define CANCEL_BUTTON_PIN 0
#endif

// Thời gian ân hạn (ms) trước khi gửi báo động khẩn cấp lên Gateway/CoreIoT (5 giây)
#define GRACE_PERIOD_MS 5000

void task_fall_detection(void *pvParameters) {
    // 1. Khởi tạo chân LED báo hiệu và nút nhấn hủy cảnh báo
    pinMode(LED_FALL_PIN, OUTPUT);
    pinMode(CANCEL_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_FALL_PIN, LOW);

    Serial.println("[MPU6050] Initializing...");
    if (!mpu.begin()) {
        Serial.println("[MPU6050] Failed to find MPU6050 chip! Task stopped.");
        vTaskDelete(NULL); 
    }
    Serial.println("[MPU6050] Found!");

    // 2. Cấu hình dải đo và bộ lọc phần cứng
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Các biến trạng thái của thuật toán 3 giai đoạn (chu kỳ lấy mẫu 100ms = 10Hz)
    bool trigger1 = false; // Pha 1: Rơi tự do (gia tốc giảm đột ngột)
    bool trigger2 = false; // Pha 2: Va đập mạnh xuống sàn
    bool trigger3 = false; // Pha 3: Thay đổi góc nghiêng đột ngột
    uint8_t trigger1_count = 0;
    uint8_t trigger2_count = 0;
    uint8_t trigger3_count = 0;

    // Các biến quản lý thời gian ân hạn & trạng thái té ngã
    bool pending_fall = false;         // Đang trong 5s đếm ngược chờ xác nhận
    unsigned long pending_start = 0;   // Thời điểm bắt đầu đếm ngược
    bool confirmed_fall = false;       // Đã xác nhận té ngã thật sự

    // 3. Vòng lặp đọc cảm biến đa nhiệm FreeRTOS (10Hz - mỗi 100ms)
    while (1) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        // Quy đổi gia tốc từ m/s^2 sang đơn vị chuẩn g (1g ≈ 9.80665 m/s^2)
        float ax_g = a.acceleration.x / 9.80665f;
        float ay_g = a.acceleration.y / 9.80665f;
        float az_g = a.acceleration.z / 9.80665f;
        float total_acc_g = sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);

        // Quy đổi vận tốc góc từ rad/s sang độ/giây (deg/s)
        float gx_deg = g.gyro.x * 57.29578f;
        float gy_deg = g.gyro.y * 57.29578f;
        float gz_deg = g.gyro.z * 57.29578f;
        float angle_change = sqrt(gx_deg * gx_deg + gy_deg * gy_deg + gz_deg * gz_deg);

        // --- THUẬT TOÁN PHÁT HIỆN TÉ NGÃ 3 GIAI ĐOẠN ---
        if (!pending_fall && !confirmed_fall) {
            // PHA 1: Rơi tự do (tổng gia tốc tụt xuống dưới 0.45g)
            if (total_acc_g <= 0.45f && !trigger2 && !trigger3) {
                trigger1 = true;
                trigger1_count = 0;
                Serial.println("[FALL] Trigger 1: Phat hien roi tu do!");
            }

            // PHA 2: Va đập mạnh trong vòng 0.6s sau khi rơi (tổng gia tốc >= 1.8g)
            if (trigger1) {
                trigger1_count++;
                if (total_acc_g >= 1.80f) {
                    trigger2 = true;
                    trigger2_count = 0;
                    trigger1 = false;
                    trigger1_count = 0;
                    Serial.println("[FALL] Trigger 2: Phat hien va dap manh!");
                } else if (trigger1_count >= 6) {
                    trigger1 = false; // Hết 0.6s không va đập -> Hủy
                    trigger1_count = 0;
                }
            }

            // PHA 3: Đổi tư thế đột ngột trong vòng 0.6s sau va đập (vận tốc góc >= 45 deg/s)
            if (trigger2) {
                trigger2_count++;
                if (angle_change >= 45.0f) {
                    trigger3 = true;
                    trigger3_count = 0;
                    trigger2 = false;
                    trigger2_count = 0;
                    Serial.println("[FALL] Trigger 3: Thay doi tu the! Kiem tra nam bat dong...");
                } else if (trigger2_count >= 6) {
                    trigger2 = false; // Không đổi tư thế -> Hủy
                    trigger2_count = 0;
                }
            }

            // XÁC NHẬN SAU PHA 3: Chờ 1 giây (10 mẫu), nếu nằm yên (angle_change <= 15 deg/s) -> Nghi ngờ té ngã
            if (trigger3) {
                trigger3_count++;
                if (trigger3_count >= 10) {
                    if (angle_change <= 15.0f) {
                        pending_fall = true;
                        pending_start = millis();
                        Serial.println("[FALL] NGHI NGO TE NGA! Bam nut BOOT trong 5s de huy neu bao gia...");
                    } else {
                        Serial.println("[FALL] Nguoi dung da tu dung day -> Huy canh bao.");
                    }
                    trigger3 = false;
                    trigger3_count = 0;
                }
            }
        }

        // --- XỬ LÝ THỜI GIAN ÂN HẠN 5 GIÂY & NÚT BẤM HỦY BÁO ĐỘNG ---
        if (pending_fall) {
            // Nhấp nháy đèn LED liên tục (chu kỳ 200ms) để cảnh báo người đeo đang đếm ngược
            digitalWrite(LED_FALL_PIN, (millis() / 200) % 2);

            // Nếu người đeo bấm nút BOOT (GPIO 0) -> Hủy báo động giả
            if (digitalRead(CANCEL_BUTTON_PIN) == LOW) {
                pending_fall = false;
                confirmed_fall = false;
                digitalWrite(LED_FALL_PIN, LOW);
                Serial.println("[FALL] DA HUY BAO DONG GIA BANG NUT NHAN!");
                vTaskDelay(pdMS_TO_TICKS(300)); // Chống dội phím
            }
            // Nếu quá 5 giây không có ai bấm hủy -> Chốt trạng thái té ngã khẩn cấp!
            else if (millis() - pending_start >= GRACE_PERIOD_MS) {
                pending_fall = false;
                confirmed_fall = true;
                digitalWrite(LED_FALL_PIN, HIGH); // Sáng đèn liên tục
                Serial.println("[FALL] XAC NHAN TE NGA KHAN CAP! Gui tin hieu len Gateway...");
            }
        }

        // Cho phép bấm nút BOOT để reset trạng thái sau khi đã xử lý xong sự cố té ngã
        if (confirmed_fall && digitalRead(CANCEL_BUTTON_PIN) == LOW) {
            confirmed_fall = false;
            digitalWrite(LED_FALL_PIN, LOW);
            Serial.println("[FALL] Da xac nhan an toan, reset trang thai he thong.");
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        // 4. Cập nhật kết quả vào biến toàn cục (có khóa Mutex bảo vệ)
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            glob_data.is_fall = confirmed_fall;
            glob_data.fall_confidence = confirmed_fall ? 0.96 : (pending_fall ? 0.65 : 0.02);
            xSemaphoreGive(xDataMutex);
        }

        // In thông số gọn ra Serial Monitor để theo dõi
        Serial.printf("[MPU6050] Acc:%.2fg | Gyro:%.1f deg/s | State: %s\n",
                      total_acc_g, angle_change,
                      confirmed_fall ? "FALL_CONFIRMED" : (pending_fall ? "PENDING_5S" : "NORMAL"));

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}




// #include "fall_detection.h"
// #include <Adafruit_MPU6050.h>
// #include <Adafruit_Sensor.h>
// #include <Wire.h>

// Adafruit_MPU6050 mpu;

// void task_fall_detection(void *pvParameters) {
//     // 1. Khởi tạo bus I2C chuẩn của YoloUNO (SDA = 11, SCL = 12)[cite: 1]
//     // Wire.begin(11, 12);
//     pinMode(LED_FALL_PIN, OUTPUT);

//     Serial.println("[MPU6050] Initializing...");
//     if (!mpu.begin()) {
//         Serial.println("[MPU6050] Failed to find MPU6050 chip! Task stopped.");
//         vTaskDelete(NULL); // Xóa task an toàn, không làm treo các task khác
//     }
//     Serial.println("[MPU6050] Found!");

//     // 2. Cấu hình dải đo và bộ lọc 
//     mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
//     mpu.setGyroRange(MPU6050_RANGE_500_DEG);
//     mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

//     vTaskDelay(pdMS_TO_TICKS(100));

//     // 3. Vòng lặp đọc cảm biến đa nhiệm FreeRTOS[cite: 3]
//     while (1) {
//         sensors_event_t a, g, temp;
//         mpu.getEvent(&a, &g, &temp);

//         // Kiểm tra điều kiện nghiêng/va đập mạnh trên trục X hoặc Y (> 7.5 m/s^2)
//         bool fall_detected = (a.acceleration.x > 7.5 || a.acceleration.x < -7.5 || 
//                               a.acceleration.y > 7.5 || a.acceleration.y < -7.5);

//         // Bật/tắt LED trên YoloUNO để báo hiệu trực quan
//         digitalWrite(LED_FALL_PIN, fall_detected ? HIGH : LOW);

//         // Cập nhật kết quả vào biến toàn cục (có khóa Mutex bảo vệ)
//         if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
//             glob_data.is_fall = fall_detected;
//             glob_data.fall_confidence = fall_detected ? 0.90 : 0.05;
//             xSemaphoreGive(xDataMutex);
//         }

//         // In thông số ra Serial Monitor để kiểm tra
//         Serial.printf("[MPU6050] Accel X:%.2f, Y:%.2f, Z:%.2f | Fall: %s\n",
//                       a.acceleration.x, a.acceleration.y, a.acceleration.z,
//                       fall_detected ? "YES" : "NO");

//         // Lấy mẫu mỗi 100ms (10Hz) để phản ứng té ngã nhanh hơn mức 1500ms cũ
//         vTaskDelay(pdMS_TO_TICKS(100));
//     }
// }

