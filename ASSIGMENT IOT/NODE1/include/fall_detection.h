#ifndef __FALL_DETECTION_H__
#define __FALL_DETECTION_H__

#include "global.h"

#define LED_FALL_PIN 48  // LED đơn tích hợp trên board YoloUNO[cite: 3]

void task_fall_detection(void *pvParameters);

#endif