#ifndef __FALL_DETECTION_H__
#define __FALL_DETECTION_H__

#include "global.h"

#define LED_FALL_PIN 48  // LED trên board YoloUNO (nếu là NeoPixel RGB thì digitalWrite không sáng - cần kiểm tra)

void task_fall_detection(void *pvParameters);

#endif
