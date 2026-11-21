// include/global.h
#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "config.h"

// Gói tin ESP-NOW theo hợp đồng giao tiếp (docs/interface.md):
// hr (bpm), spo2 (%), fall (0/1), valid (0/1: có ngón tay), seq (số thứ tự gói).
// Node 2 phải dùng đúng struct này.
typedef struct __attribute__((packed)) {
    uint8_t  hr;     // nhịp tim, bpm
    uint8_t  spo2;   // SpO2, %
    uint8_t  fall;   // 1 = đã xác nhận ngã
    uint8_t  valid;  // 1 = HR/SpO2 hợp lệ (có ngón tay)
    uint32_t seq;    // tăng dần mỗi gói để Node 2 phát hiện mất gói
} SensorPacket;

// Dữ liệu cảm biến dùng chung giữa các task (seq do task gửi tự quản lý)
extern SensorPacket glob_data;
extern SemaphoreHandle_t xDataMutex;  // bảo vệ glob_data
extern SemaphoreHandle_t xI2CMutex;   // bảo vệ bus I2C dùng chung MAX30102 + MPU6050
extern TaskHandle_t xEspNowTaskHandle; // để đánh thức task gửi khi có ngã
extern uint8_t node2_mac_addr[6];

#endif
