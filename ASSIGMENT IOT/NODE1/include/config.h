// include/config.h - cấu hình Node 1 (thiết bị đeo)
#ifndef __CONFIG_H__
#define __CONFIG_H__

// ---------- ESP-NOW ----------
// MAC của Node 2. Hiện để broadcast (FF:..:FF) để test; khi có board thật của B
// thì thay bằng MAC in ra từ Node 2.
#define NODE2_MAC {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

// Kênh WiFi dùng cho ESP-NOW. PHẢI trùng kênh Node 2 (và router nếu Node 2 nối WiFi).
#define ESPNOW_CHANNEL 1

// Chu kỳ gửi gói bình thường (ms). Hợp đồng: 1-2 s. Sự kiện ngã gửi ngay.
#define ESPNOW_SEND_PERIOD_MS 1000

// 1 = gửi dữ liệu giả (HR/SpO2 ngẫu nhiên) để B test nhận khi chưa có cảm biến thật
#define USE_FAKE_DATA 0

// ---------- I2C ----------
#define I2C_SDA_PIN 11
#define I2C_SCL_PIN 12

// ---------- MAX30102 ----------
#define FINGER_IR_THRESHOLD 50000   // IR dưới mức này = không có ngón tay
#define HR_TIMEOUT_MS       5000    // quá lâu không có nhịp mới -> HR không hợp lệ

// ---------- MPU6050 / phát hiện ngã ----------
#define MPU_SAMPLE_PERIOD_MS 20     // 50 Hz
#define FREEFALL_G_THRESH    0.45f  // pha 1: SVM tụt xuống dưới mức này
#define IMPACT_G_THRESH      1.80f  // pha 2: đỉnh va đập
#define GYRO_CHANGE_THRESH   45.0f  // pha 3: đổi tư thế (deg/s)
#define STILL_GYRO_THRESH    15.0f  // coi là nằm yên (deg/s)
#define PHASE_WINDOW_MS      600    // thời gian chờ giữa các pha
#define STILL_CHECK_MS       1000   // thời gian kiểm tra bất động sau pha 3
#define GRACE_PERIOD_MS      5000   // thời gian ân hạn bấm nút hủy báo giả

// ---------- Chế độ in Serial ----------
// 0 = log trạng thái dễ đọc (mặc định)
// 1 = in "ms,ax,ay,az,svm" mỗi mẫu 50 Hz để vẽ đồ thị (dùng tools/plot_svm.py)
#define SERIAL_OUTPUT_MODE 0

#endif
