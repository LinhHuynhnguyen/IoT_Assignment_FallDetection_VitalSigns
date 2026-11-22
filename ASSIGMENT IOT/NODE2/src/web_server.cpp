#include "web_server.h"
#include <WiFi.h>
#include <WebServer.h>

#define BTN_BOOT_PIN 0
static constexpr uint8_t esp_now_channel = 1;
WebServer server(80);

bool isAPMode = true;
bool connecting = false;
unsigned long connect_start_ms = 0;

// Giao diện HTML Dashboard cục bộ (T07)
String getDashboardHTML() {
    String html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Health Monitor Local</title>
<style>
  body { font-family: Arial; text-align: center; background: #f0f4f8; padding: 20px; }
  .card { background: white; max-width: 400px; margin: auto; padding: 20px; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
  .val { font-size: 28px; font-weight: bold; color: #007bff; }
  .danger { color: red; font-weight: bold; }
  button { padding: 12px 24px; font-size: 16px; border: none; border-radius: 8px; cursor: pointer; margin: 10px; }
  .btn-buzz { background: #dc3545; color: white; }
  .btn-set { background: #6c757d; color: white; }
</style></head><body>
<div class="card">
  <h2>Giám Sát Sinh Tồn (Local)</h2>
  <p>Nhịp tim: <span id="hr" class="val">--</span> bpm</p>
  <p>SpO2: <span id="spo2" class="val">--</span> %</p>
  <p>Trạng thái: <span id="status">Bình thường</span></p>
  <button class="btn-buzz" onclick="toggleBuzzer()">Bật / Tắt Còi</button>
  <br><a href="/settings"><button class="btn-set">Cài đặt Wi-Fi</button></a>
</div>
<script>
  setInterval(() => {
    fetch('/sensors').then(r => r.json()).then(d => {
      document.getElementById('hr').innerText = d.hr.toFixed(1);
      document.getElementById('spo2').innerText = d.spo2.toFixed(1);
      let st = document.getElementById('status');
      st.innerText = d.fall ? "CẢNH BÁO TÉ NGÃ!" : "Bình thường";
      st.className = d.fall ? "danger" : "";
    });
  }, 2000);
  function toggleBuzzer() { fetch('/toggle'); }
</script></body></html>
)rawliteral";
    return html;
}

// Giao diện cài đặt Wi-Fi 
String getSettingsHTML() {
    return R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Wi-Fi Setup</title></head><body style="font-family:Arial;text-align:center;padding:30px;">
  <h2>Cấu hình Wi-Fi Router</h2>
  <form action="/connect" method="get">
    <input name="ssid" placeholder="Tên Wi-Fi (2.4GHz)" required style="padding:10px;width:80%;margin:5px;"><br>
    <input name="pass" type="password" placeholder="Mật khẩu Wi-Fi" style="padding:10px;width:80%;margin:5px;"><br>
    <button type="submit" style="padding:10px 30px;background:#28a745;color:white;border:none;border-radius:5px;">Kết nối</button>
  </form>
</body></html>
)rawliteral";
}

void startAP() {
    WiFi.softAP(ap_ssid.c_str(), ap_password.c_str(), esp_now_channel);
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
    isAPMode = true;
    connecting = false;
    isWifiConnected = false;
}

void setupServer() {
    server.on("/", []() { server.send(200, "text/html", getDashboardHTML()); });
    server.on("/settings", []() { server.send(200, "text/html", getSettingsHTML()); });
    server.on("/sensors", []() {
        String json = "{\"hr\":" + String(glob_hr) + ",\"spo2\":" + String(glob_spo2) +
                      ",\"fall\":" + String(glob_is_fall ? "true" : "false") + "}";
        server.send(200, "application/json", json);
    });
    // server.on("/toggle", []() {
    //     glob_buzzer_state = !glob_buzzer_state;
    //     server.send(200, "text/plain", "OK");
    // });


    server.on("/toggle", []() {
        glob_buzzer_state = !glob_buzzer_state;
        glob_buzzer_muted = !glob_buzzer_state; // Tắt còi thì bật cờ khóa tiếng luôn
        server.send(200, "text/plain", "OK");
    });

    server.on("/connect", []() {
        wifi_ssid = server.arg("ssid");
        wifi_password = server.arg("pass");
        server.send(200, "text/plain", "Connecting to " + wifi_ssid + "...");
        WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
        connecting = true;
        connect_start_ms = millis();
    });
    server.begin();
}

void main_server_task(void *pvParameters) {
    pinMode(BTN_BOOT_PIN, INPUT_PULLUP);
    startAP();
    setupServer();

    while (1) {
        server.handleClient();

        // Nhấn BOOT quay về AP 
        if (digitalRead(BTN_BOOT_PIN) == LOW) {
            vTaskDelay(pdMS_TO_TICKS(100));
            if (digitalRead(BTN_BOOT_PIN) == LOW && !isAPMode) startAP();
        }

        // Giám sát kết nối STA 
        if (connecting) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.print("STA IP: "); Serial.println(WiFi.localIP());
                const int router_channel = WiFi.channel();
                Serial.printf("[WiFi] Router channel: %d\n", router_channel);
                if (router_channel != esp_now_channel) {
                    Serial.printf("[ESP-NOW] Set Node 1 to channel %d to match the router.\n", router_channel);
                }
                isWifiConnected = true;
                isAPMode = false;
                connecting = false;
                xSemaphoreGive(xBinarySemaphoreInternet); // Báo hiệu có Internet!
            } else if (millis() - connect_start_ms > 10000) {
                Serial.println("Connect failed! Back to AP.");
                startAP();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
