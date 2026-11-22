#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Dữ liệu cảm biến nhận từ Node 1
extern float glob_hr;
extern float glob_spo2;
extern bool glob_is_fall;
extern unsigned long glob_last_seen_ms;
extern bool glob_sensor_valid;

// Trạng thái Còi & Ngưỡng cảnh báo
extern bool glob_buzzer_state;
extern bool glob_buzzer_muted;
extern float glob_hr_max_threshold;
extern float glob_spo2_min_threshold;

// Cấu hình Wi-Fi
extern String ap_ssid;
extern String ap_password;
extern String wifi_ssid;
extern String wifi_password;

// Đồng bộ trạng thái mạng
extern bool isWifiConnected;
extern SemaphoreHandle_t xBinarySemaphoreInternet;

#endif