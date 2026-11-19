// include/global.h
#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Cấu trúc gói tin truyền qua ESP-NOW (Phải giống hệt bên Node 2)
typedef struct __attribute__((packed)) {
    float heart_rate;
    float spo2;
    bool is_fall;
    float fall_confidence;
} SensorPacket;

extern SensorPacket glob_data;
extern SemaphoreHandle_t xDataMutex; // Bảo vệ biến glob_data
extern uint8_t node2_mac_addr[6];    // Địa chỉ MAC của Node 2

#endif