# Node 1 – Thiết bị đeo (MAX30102 + MPU6050 + ESP-NOW)

Board: YoloUNO (ESP32-S3), PlatformIO + Arduino. Node 1 đo nhịp tim/SpO2, phát hiện ngã và gửi gói ESP-NOW cho Node 2.

## Cấu trúc
| File | Nhiệm vụ |
|---|---|
| `include/config.h` | MAC Node 2, kênh ESP-NOW, ngưỡng ngã, chế độ Serial |
| `include/global.h` | Struct gói tin `SensorPacket` (theo hợp đồng giao tiếp) |
| `src/sensor_vitals.cpp` | Đọc MAX30102: HR (trung bình 4 nhịp), SpO2 (ước lượng AC/DC), cờ `valid` |
| `src/fall_detection.cpp` | Đọc MPU6050 50 Hz, SVM, phát hiện ngã 3 pha + nút BOOT hủy báo giả |
| `src/espnow_sender.cpp` | Gửi ESP-NOW (seq, callback log, gửi ngay khi ngã) |
| `tools/plot_svm.py` | Thu và vẽ đồ thị SVM |

## Gói tin ESP-NOW
`{uint8 hr, uint8 spo2, uint8 fall, uint8 valid, uint32 seq}` (packed, 8 byte). Node 2 phải dùng đúng struct này.

## Đấu nối
MAX30102 và MPU6050 chung bus I2C: SDA = GPIO 11, SCL = GPIO 12, cấp 3.3 V. Địa chỉ khác nhau (0x57 / 0x68).

## Cấu hình & chạy
1. Sửa `include/config.h`: `NODE2_MAC` (mặc định broadcast), `ESPNOW_CHANNEL` (trùng Node 2).
2. Build/nạp bằng PlatformIO, mở Serial Monitor 115200.
3. `USE_FAKE_DATA 1` để gửi dữ liệu giả (cho Node 2 test khi chưa có cảm biến).
4. `SERIAL_OUTPUT_MODE 1` để in CSV `ms,ax,ay,az,svm`, rồi dùng `tools/plot_svm.py` vẽ đồ thị.

## Ghi chú
- SpO2 là ước lượng từ tỉ số AC/DC, chưa hiệu chuẩn y tế; hệ thống chỉ hỗ trợ giám sát.
- Test ngã chỉ làm trên nệm dày, không ngã thật.
