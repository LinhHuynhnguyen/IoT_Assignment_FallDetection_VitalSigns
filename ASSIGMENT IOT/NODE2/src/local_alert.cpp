#include "local_alert.h"

#include <Wire.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 display(128, 64, &Wire, -1);

#ifdef NODE2_PASSIVE_BUZZER
static constexpr uint8_t buzzer_pwm_channel = 0;
static constexpr uint8_t buzzer_pwm_bits = 10;
static constexpr unsigned buzzer_frequency_hz = 2000;
#endif

static bool showOledReady() {
    constexpr uint8_t address = 0x3C;

    Wire.begin(11, 12);
    Wire.beginTransmission(address);
    if (Wire.endTransmission() != 0 ||
        !display.begin(SSD1306_SWITCHCAPVCC, address, false, false)) {
        Serial.println("[OLED] Init failed");
        return false;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("NODE 2 READY");
    display.display();

    return true;
}

static void updateOled() {
    display.clearDisplay();
    display.setCursor(0, 0);

    unsigned long last_seen = glob_last_seen_ms;
    if (last_seen == 0) {
        display.println("Wait Node 1");
    } else if (millis() - last_seen >= 5000) {
        display.println("Node 1 Offline");
    } else {
        if (glob_sensor_valid) {
            display.printf("HR: %.1f bpm\n", glob_hr);
            display.printf("SpO2: %.1f%%\n", glob_spo2);
        } else {
            display.println("HR: -- bpm");
            display.println("SpO2: --%");
        }
        display.println(glob_is_fall ? "FALL!" : "Normal");
    }

    display.display();
}

void task_local_alert(void *pvParameters) {
#ifdef NODE2_PASSIVE_BUZZER
    ledcSetup(buzzer_pwm_channel, buzzer_frequency_hz, buzzer_pwm_bits);
    ledcWrite(buzzer_pwm_channel, 0);
    ledcAttachPin(BUZZER_PIN, buzzer_pwm_channel);
#else
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
#endif

    bool oled_ready = showOledReady();

    while (1) {
        // Kiểm tra điều kiện nguy hiểm (té ngã hoặc nhịp tim / SpO2 vượt ngưỡng)
        bool danger = glob_is_fall || 
                      (glob_hr > glob_hr_max_threshold) || 
                      (glob_spo2 < glob_spo2_min_threshold && glob_spo2 > 0);

        if (danger) {
            if (!glob_buzzer_muted) {
                glob_buzzer_state = true;
            }
        } else {
            // Hết nguy hiểm -> reset cờ khóa tiếng để sẵn sàng cho lần sau
            glob_buzzer_muted = false;
        }

        // Xuất tín hiệu ra chân còi thực tế
#ifdef NODE2_PASSIVE_BUZZER
        constexpr uint32_t buzzer_half_duty = 1U << (buzzer_pwm_bits - 1);
        ledcWrite(buzzer_pwm_channel, glob_buzzer_state ? buzzer_half_duty : 0);
#else
        digitalWrite(BUZZER_PIN, glob_buzzer_state ? HIGH : LOW);
#endif

        if (oled_ready) {
            updateOled();
        }

        vTaskDelay(pdMS_TO_TICKS(200));

    }
}
