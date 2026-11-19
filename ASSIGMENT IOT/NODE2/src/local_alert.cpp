#include "local_alert.h"

void task_local_alert(void *pvParameters) {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

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
        digitalWrite(BUZZER_PIN, glob_buzzer_state ? HIGH : LOW);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}