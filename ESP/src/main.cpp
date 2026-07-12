#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_sleep.h>

const char* WIFI_SSID = "REDACTED_WIFI_SSID";
const char* WIFI_PASS = "REDACTED_WIFI_PASSWORD";

const char* MQTT_SERVER = "REDACTED_MQTT_HOST";
const int   MQTT_PORT   = 1883;
const char* MQTT_USER   = "esp32";
const char* MQTT_PASS   = "REDACTED_MQTT_PASSWORD";

const int DEVICE_ID = 1;

WiFiClient espClient;
PubSubClient client(espClient);
TFT_eSPI tft = TFT_eSPI();

#define RXD2 16
#define TXD2 17
#define WAKE_STM_PIN 25
#define WAKE_BUTTON_PIN GPIO_NUM_33

// Диагностический LED (внешний, через резистор на GND). Активный HIGH.
// Пины 2 и 4 заняты TFT (DC/RST), поэтому берём GPIO 26 — свободен и
// безопасен, RTC-домен deep_sleep его не трогает.
#define DIAG_LED_PIN 26

const uint64_t SLEEP_INTERVAL_US = 30ULL * 60ULL * 1000000ULL;
const unsigned long WAKE_PULSE_MS = 100;
const unsigned long MEASURE_TIMEOUT_MS = 20000;
const unsigned long MQTT_FLUSH_MS = 500;

// -------------------- DIAG LED --------------------

// Мигает N раз с короткими импульсами. Схема:
//   Проснулся                → 1
//   UART от STM не пришёл    → 3
//   MQTT публикация OK       → 1
//   MQTT публикация FAIL     → 2
void diagBlink(uint8_t count)
{
  for (uint8_t i = 0; i < count; i++) {
    digitalWrite(DIAG_LED_PIN, HIGH);
    delay(120);
    digitalWrite(DIAG_LED_PIN, LOW);
    delay(200);
  }
  delay(400);
}

// -------------------- DISPLAY --------------------

void screenHeader(const String& text)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println(text);
}

void screenLine(const String& text, int y)
{
  tft.setCursor(10, y);
  tft.println(text);
}

// -------------------- WIFI --------------------

void connectWiFi()
{
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected!");
  Serial.println(WiFi.localIP());

  screenLine("WiFi OK", 40);
}

// -------------------- MQTT --------------------

void connectMQTT()
{
  while (!client.connected()) {

    String clientId = "esp32-" + String(DEVICE_ID);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {

      Serial.println("MQTT connected");
      screenLine("MQTT OK", 70);
    } else {
      Serial.println("MQTT connection failed");
      delay(2000);
    }
  }
}

String readMeasurement()
{
  unsigned long startedAt = millis();

  while (millis() - startedAt < MEASURE_TIMEOUT_MS) {
    if (Serial2.available()) {
      String line = Serial2.readStringUntil('\n');
      line.trim();

      Serial.print("UART: ");
      Serial.println(line);

      if (line.startsWith("{") && line.endsWith("}")) {
        return line;
      }
    }

    delay(10);
  }

  return "";
}

String topicForPayload(const String& payload)
{
  const String key = "\"deviceId\":";
  int keyPos = payload.indexOf(key);

  if (keyPos < 0) {
    return "smartgarden/" + String(DEVICE_ID) + "/data";
  }

  int valueStart = keyPos + key.length();
  while (valueStart < payload.length() && payload[valueStart] == ' ') {
    valueStart++;
  }

  int valueEnd = valueStart;
  while (valueEnd < payload.length() && isDigit(payload[valueEnd])) {
    valueEnd++;
  }

  if (valueEnd == valueStart) {
    return "smartgarden/" + String(DEVICE_ID) + "/data";
  }

  return "smartgarden/" + payload.substring(valueStart, valueEnd) + "/data";
}

void goToSleep()
{
  screenLine("SLEEP 30 MIN", 160);
  Serial.println("Going to deep sleep for 30 minutes");

  client.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  digitalWrite(WAKE_STM_PIN, LOW);
  esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
  esp_sleep_enable_ext0_wakeup(WAKE_BUTTON_PIN, 0);
  delay(100);
  esp_deep_sleep_start();
}

void requestMeasurement()
{
  digitalWrite(WAKE_STM_PIN, HIGH);
  delay(WAKE_PULSE_MS);
  digitalWrite(WAKE_STM_PIN, LOW);

  Serial.println("STM32 wake pulse sent");
}

// -------------------- SETUP --------------------

void setup()
{
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  pinMode(WAKE_STM_PIN, OUTPUT);
  digitalWrite(WAKE_STM_PIN, LOW);
  pinMode((uint8_t)WAKE_BUTTON_PIN, INPUT_PULLUP);

  pinMode(DIAG_LED_PIN, OUTPUT);
  digitalWrite(DIAG_LED_PIN, LOW);
  diagBlink(1);  // проснулся

  tft.init();
  tft.setRotation(1);
  screenHeader("SMART GARDEN");

  connectWiFi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  connectMQTT();

  requestMeasurement();

  String line = readMeasurement();

  if (line.length() > 0) {
    screenHeader("DATA");
    screenLine(line, 40);

    String topic = topicForPayload(line);
    bool ok = client.publish(topic.c_str(), line.c_str(), false);

    if (ok) {
      screenLine("MQTT SENT", 120);
      Serial.print("MQTT SENT: ");
      Serial.println(topic);
      diagBlink(1);  // отправлено на сервер
    } else {
      screenLine("MQTT FAIL", 120);
      Serial.println("MQTT FAIL");
      diagBlink(2);  // MQTT провалился
    }
  } else {
    screenHeader("NO DATA");
    Serial.println("No valid STM32 JSON received before timeout");
    diagBlink(3);  // UART от STM не пришёл
  }

  unsigned long flushUntil = millis() + MQTT_FLUSH_MS;
  while (millis() < flushUntil) {
    client.loop();
    delay(10);
  }

  goToSleep();
}

// -------------------- LOOP --------------------

void loop()
{
}
