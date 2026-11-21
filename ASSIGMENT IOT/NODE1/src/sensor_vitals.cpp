#include "sensor_vitals.h"
#include <Wire.h>
#include <math.h>
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 particleSensor;

// ---- Nhịp tim: trung bình trượt qua RATE_SIZE nhịp gần nhất ----
static const byte RATE_SIZE = 4;
static float rates[RATE_SIZE];
static byte rateSpot = 0;
static byte beatCount = 0;        // số nhịp hợp lệ đã thu kể từ lúc đặt ngón tay
static unsigned long lastBeat = 0; // 0 = chưa có nhịp trước đó
static float beatAvg = 0;

// ---- SpO2: ước lượng từ tỉ số AC/DC của Red và IR trên cửa sổ trượt ----
// R = (AC_red/DC_red) / (AC_ir/DC_ir);  SpO2 ~ 110 - 25*R (công thức xấp xỉ kinh điển).
// Đây là ƯỚC LƯỢNG chưa hiệu chuẩn y tế, chỉ dùng để giám sát/minh họa.
static const int SPO2_WIN = 100;   // 100 mẫu ~ 1 s ở 100 Hz
static float redBuf[SPO2_WIN];
static float irBuf[SPO2_WIN];
static int winPos = 0;
static int winFill = 0;
static int sinceCalc = 0;
static float spo2Avg = 0;          // đã làm mượt
static byte spo2Count = 0;

static void resetState() {
    rateSpot = 0; beatCount = 0; lastBeat = 0; beatAvg = 0;
    winPos = 0; winFill = 0; sinceCalc = 0; spo2Avg = 0; spo2Count = 0;
}

static void pushSample(float red, float ir) {
    redBuf[winPos] = red;
    irBuf[winPos] = ir;
    winPos = (winPos + 1) % SPO2_WIN;
    if (winFill < SPO2_WIN) winFill++;
    sinceCalc++;
}

// Tính SpO2 mỗi 25 mẫu khi cửa sổ đã đầy
static void updateSpo2() {
    if (winFill < SPO2_WIN || sinceCalc < 25) return;
    sinceCalc = 0;

    float dcR = 0, dcI = 0;
    for (int i = 0; i < SPO2_WIN; i++) { dcR += redBuf[i]; dcI += irBuf[i]; }
    dcR /= SPO2_WIN; dcI /= SPO2_WIN;
    if (dcR < 1 || dcI < 1) return;

    float acR = 0, acI = 0;
    for (int i = 0; i < SPO2_WIN; i++) {
        float dr = redBuf[i] - dcR, di = irBuf[i] - dcI;
        acR += dr * dr; acI += di * di;
    }
    acR = sqrtf(acR / SPO2_WIN);
    acI = sqrtf(acI / SPO2_WIN);
    if (acI < 1) return;

    float R = (acR / dcR) / (acI / dcI);
    float s = constrain(110.0f - 25.0f * R, 70.0f, 100.0f);
    spo2Avg = (spo2Count == 0) ? s : (0.8f * spo2Avg + 0.2f * s); // lọc mượt
    if (spo2Count < 255) spo2Count++;
}

void task_read_vitals(void *pvParameters) {
    Serial.println("[MAX30102] Initializing...");

    bool ok = false;
    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        ok = particleSensor.begin(Wire, I2C_SPEED_FAST);
        if (ok) {
            // LED 0x1F (~6 mA), trung bình 4 mẫu, chế độ Red+IR, 400 Hz -> ~100 Hz hiệu dụng
            particleSensor.setup(0x1F, 4, 2, 400, 411, 4096);
            // Red và IR cùng độ sáng để tỉ số Red/IR không bị lệch
            particleSensor.setPulseAmplitudeRed(0x1F);
            particleSensor.setPulseAmplitudeIR(0x1F);
            particleSensor.setPulseAmplitudeGreen(0);
        }
        xSemaphoreGive(xI2CMutex);
    }
    if (!ok) {
        Serial.println("[MAX30102] Was not found. Please check wiring/power.");
        vTaskDelete(NULL);
    }
    Serial.println("[MAX30102] Place your index finger on the sensor with steady pressure.");

    unsigned long lastPrintMs = 0;
    const int MAXB = 32;
    uint32_t redS[MAXB], irS[MAXB];
    long lastIr = 0;

    while (1) {
        // Đọc hết các mẫu mới trong FIFO (giữ mutex I2C thật ngắn)
        int n = 0;
        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            particleSensor.check();
            while (particleSensor.available() && n < MAXB) {
                redS[n] = particleSensor.getFIFORed();
                irS[n]  = particleSensor.getFIFOIR();
                particleSensor.nextSample();
                n++;
            }
            xSemaphoreGive(xI2CMutex);
        }

        for (int i = 0; i < n; i++) {
            long ir = (long)irS[i];
            lastIr = ir;

            if (ir < FINGER_IR_THRESHOLD) {
                if (beatCount > 0 || winFill > 0) resetState(); // rút ngón tay -> xóa trạng thái
                continue;
            }

            pushSample((float)redS[i], (float)ir);
            updateSpo2();

            if (checkForBeat(ir)) {
                unsigned long now = millis();
                if (lastBeat != 0) {
                    float bpm = 60.0f / ((now - lastBeat) / 1000.0f);
                    if (bpm > 20 && bpm < 255) {
                        rates[rateSpot++] = bpm;
                        rateSpot %= RATE_SIZE;
                        if (beatCount < RATE_SIZE) beatCount++;
                        // Chỉ lấy trung bình trên số nhịp đã có (tránh bị kéo xuống bởi giá trị 0)
                        float sum = 0;
                        for (byte k = 0; k < beatCount; k++) sum += rates[k];
                        beatAvg = sum / beatCount;
                    }
                }
                lastBeat = now;
            }
        }

        // valid = có ngón tay + đủ RATE_SIZE nhịp + còn nhịp mới + SpO2 đã có giá trị
        bool finger = (lastIr >= FINGER_IR_THRESHOLD);
        bool valid = finger
                  && beatCount >= RATE_SIZE
                  && (millis() - lastBeat) < HR_TIMEOUT_MS
                  && spo2Count > 0;

        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            glob_data.valid = valid ? 1 : 0;
            glob_data.hr    = valid ? (uint8_t)constrain((int)(beatAvg + 0.5f), 0, 255) : 0;
            glob_data.spo2  = valid ? (uint8_t)constrain((int)(spo2Avg + 0.5f), 0, 100) : 0;
            xSemaphoreGive(xDataMutex);
        }

#if SERIAL_OUTPUT_MODE == 0
        if (millis() - lastPrintMs >= 500) {
            lastPrintMs = millis();
            Serial.printf("[MAX30102] IR=%ld HR=%.0f SpO2=%.1f beats=%u valid=%d%s\n",
                          lastIr, beatAvg, spo2Avg, beatCount, valid ? 1 : 0,
                          finger ? "" : " (no finger)");
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
