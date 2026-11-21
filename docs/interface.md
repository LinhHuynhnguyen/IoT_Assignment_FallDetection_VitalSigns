# Hợp đồng giao tiếp (interface)

Tài liệu chung của nhóm: Node 1 (A), Node 2 (B), CoreIOT/AI (C). Mọi thay đổi phải báo cả nhóm và sửa file này trước khi sửa code.

```
Node 1 (A) --ESP-NOW--> Node 2 (B) --MQTT/WiFi--> CoreIOT (C)
   |                                                  ^
   +--- CSV dữ liệu huấn luyện ----------------> ml/ (C)
```

## 1. ESP-NOW: Node 1 → Node 2

Gói tin dạng struct packed, 8 byte, little-endian (mặc định ESP32):

| Trường | Kiểu | Ý nghĩa |
|---|---|---|
| `hr` | `uint8_t` | Nhịp tim (bpm). 0 khi `valid = 0` |
| `spo2` | `uint8_t` | SpO2 (%). 0 khi `valid = 0` |
| `fall` | `uint8_t` | 1 = đã xác nhận ngã, 0 = bình thường |
| `valid` | `uint8_t` | 1 = HR/SpO2 hợp lệ (có ngón tay, đủ dữ liệu), 0 = không hợp lệ |
| `seq` | `uint32_t` | Số thứ tự gói, tăng 1 mỗi gói, bắt đầu từ 0 |

Định nghĩa C (nguồn chuẩn: `ASSIGMENT IOT/NODE1/include/global.h`, Node 2 phải copy y hệt):

```c
typedef struct __attribute__((packed)) {
    uint8_t  hr;
    uint8_t  spo2;
    uint8_t  fall;
    uint8_t  valid;
    uint32_t seq;
} SensorPacket;   // sizeof = 8
```

Quy tắc:
- Gửi mỗi 1 s (`ESPNOW_SEND_PERIOD_MS`). Khi ngã xác nhận thì gửi ngay, rồi gửi lại mỗi 200 ms cho tới khi người đeo bấm nút reset (`fall` về 0).
- Node 2 kiểm tra `len == sizeof(SensorPacket)`, dùng `seq` để đếm mất gói (`seq` nhảy cách > 1).
- Node 2 coi dữ liệu là mất kết nối nếu quá N giây không nhận được gói nào.
- Kênh WiFi ESP-NOW: **kênh 1** (`ESPNOW_CHANNEL` ở Node 1). Node 2 phải cùng kênh. Nếu Node 2 nối router ở kênh khác thì phải chốt lại (xem mục 5).
- Không mã hóa (`encrypt = false`).

> **Trạng thái:** Node 1 đã dùng struct mới. Node 2 (`espnow_receiver.cpp`) hiện vẫn dùng struct cũ `{float hr, float spo2, bool is_fall, float fall_confidence}` nên sẽ bỏ qua gói mới (độ dài khác). B cần đổi.

## 2. Telemetry: Node 2 → CoreIOT (MQTT)

JSON gửi lên CoreIOT:

```json
{"heart_rate":72,"spo2":97,"fall_detected":0,"sensor_ok":1}
```

| Khóa | Lấy từ | Ghi chú |
|---|---|---|
| `heart_rate` | `hr` | bpm |
| `spo2` | `spo2` | % |
| `fall_detected` | `fall` | 0/1 |
| `sensor_ok` | `valid` | 0/1 |

## 3. Điều khiển: CoreIOT → Node 2

- **RPC** `setBuzzer`: bật/tắt còi. Tham số: bật hay tắt (C và B chốt định dạng, ví dụ `true/false`).
- **Thuộc tính chia sẻ (shared attributes):**

| Khóa | Ý nghĩa |
|---|---|
| `hr_max` | Ngưỡng nhịp tim cao nhất |
| `hr_min` | Ngưỡng nhịp tim thấp nhất |
| `spo2_min` | Ngưỡng SpO2 thấp nhất |
| `night_mode` | Chế độ ban đêm (0/1) |

> Lưu ý: code Node 2 hiện đặt tên biến `glob_hr_max_threshold`, `glob_spo2_min_threshold` và chưa có `hr_min`, `night_mode`. Tên khóa trên CoreIOT vẫn theo bảng này.

## 4. Dữ liệu huấn luyện: Node 1 → C

File CSV, một dòng mỗi mẫu, đơn vị gia tốc là g, lấy mẫu 50 Hz:

```
time,ax,ay,az,label
```

Nhãn (6 lớp): `walk` (đi bộ), `sit` (ngồi), `stand_up` (đứng lên), `lie_down` (nằm xuống), `stairs` (leo cầu thang), `fall` (ngã).

Lưu vào `ml/data/`, đặt tên có nhãn, người thu và số thứ tự, ví dụ `fall_A_01.csv`.

> **Trạng thái:** firmware Node 1 hiện có chế độ in `ms,ax,ay,az,svm` (để vẽ đồ thị), **chưa có cột `label`**. Chế độ thu dữ liệu có nhãn sẽ làm sau (hạn 13/12). Nhóm chốt giúp: `time` tính bằng ms hay giây? Mặc định hiện là ms.

## 5. Các điểm cần cả nhóm chốt

- [ ] Kênh WiFi (đề xuất kênh 1, hoặc dùng kênh của router).
- [ ] MAC của Node 2 (B gửi cho A; hiện Node 1 để broadcast `FF:FF:FF:FF:FF:FF`).
- [ ] Số giây không nhận gói thì coi Node 1 mất kết nối.
- [ ] Định dạng tham số RPC `setBuzzer`.
- [ ] Đơn vị cột `time` trong CSV huấn luyện.
- [ ] Giá trị `valid = 0` thì Node 2 có gửi `heart_rate`, `spo2` bằng 0 lên CoreIOT hay bỏ qua khóa.

## 6. Lịch sử thay đổi

| Ngày | Thay đổi | Người |
|---|---|---|
| 21/11/2026 | Tạo tài liệu; struct ESP-NOW đổi sang `{hr, spo2, fall, valid, seq}` | A |
