// src/global.cpp
#include "global.h"

SensorPacket glob_data = {0.0, 0.0, false, 0.0};
SemaphoreHandle_t xDataMutex = NULL;
// Thay bằng địa chỉ MAC thực tế của ESP32 Node 2
uint8_t node2_mac_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};