// src/global.cpp
#include "global.h"

SensorPacket glob_data = {0, 0, 0, 0, 0};
SemaphoreHandle_t xDataMutex = NULL;
SemaphoreHandle_t xI2CMutex = NULL;
TaskHandle_t xEspNowTaskHandle = NULL;
uint8_t node2_mac_addr[6] = NODE2_MAC;
