#include "global.h"

float glob_hr = 0.0;
float glob_spo2 = 0.0;
bool glob_is_fall = false;
unsigned long glob_last_seen_ms = 0;
bool glob_sensor_valid = false;

bool glob_buzzer_state = false;
bool glob_buzzer_muted = false;
float glob_hr_max_threshold = 120.0;
float glob_spo2_min_threshold = 92.0;

String ap_ssid = "ESP32-HEALTH-GW";
String ap_password = "12345678";
String wifi_ssid = "";
String wifi_password = "";

bool isWifiConnected = false;
SemaphoreHandle_t xBinarySemaphoreInternet = NULL;