#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_sleep.h>
#include "secrets.h"

const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASS = SECRET_WIFI_PASS;

const char* MQTT_SERVER = SECRET_MQTT_SERVER;
const int   MQTT_PORT   = 1883;
const char* MQTT_USER   = "esp32";
const char* MQTT_PASS   = SECRET_MQTT_PASS;

const int DEVICE_ID = 2;

WiFiClient espClient;
PubSubClient client(espClient);
TFT_eSPI tft = TFT_eSPI();

#define RXD2 16
#define TXD2 17
#define WAKE_BUTTON_PIN GPIO_NUM_33

// External diagnostic LED connected through a resistor to GND. Active HIGH.
// GPIO 2 and 4 are used by the TFT (DC/RST), so GPIO 26 is used instead.
// It remains unaffected by the RTC domain during deep sleep.
#define DIAG_LED_PIN 26

const uint64_t SLEEP_INTERVAL_US = 60ULL * 60ULL * 1000000ULL;
const unsigned long MEASURE_TIMEOUT_MS = 60000;
const unsigned long MQTT_FLUSH_MS = 500;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long MQTT_CONNECT_TIMEOUT_MS = 15000;
const unsigned long MQTT_RETRY_DELAY_MS = 2000;

// -------------------- DIAG LED --------------------

// Blink codes:
//   Wake-up                 -> 1 blink
//   No UART data from STM32 -> 3 blinks
//   MQTT publish success    -> 1 blink
//   MQTT publish failure    -> 2 blinks
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

bool connectWiFi()
{
  IPAddress local_IP(192, 168, 178, 100);
  IPAddress gateway(192, 168, 178, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(192, 168, 178, 1);
  WiFi.config(local_IP, gateway, subnet, dns);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long startedAt = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection timed out");
    screenLine("WiFi timeout", 40);
    return false;
  }

  Serial.println();
  Serial.println("WiFi connected!");
  Serial.println(WiFi.localIP());

  screenLine("WiFi OK", 40);
  return true;
}

// -------------------- MQTT --------------------

bool connectMQTT()
{
  unsigned long startedAt = millis();
  while (!client.connected() && millis() - startedAt < MQTT_CONNECT_TIMEOUT_MS) {

    String clientId = "esp32-" + String(DEVICE_ID);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {

      Serial.println("MQTT connected");
      screenLine("MQTT OK", 70);
    } else {
      Serial.println("MQTT connection failed");
      delay(MQTT_RETRY_DELAY_MS);
    }
  }

  if (!client.connected()) {
    Serial.println("MQTT connection timed out");
    screenLine("MQTT timeout", 70);
    return false;
  }

  return true;
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

// Forward STM32 startup diagnostics (for example "ADS1115 OK" or "not found")
// to the ESP32 USB serial monitor. This runs before the MEASURE command is sent,
// so diagnostic lines do not interfere with the JSON measurement payload.
void printStmStartupDiagnostics()
{
  const unsigned long timeoutMs = 800;
  unsigned long lastReceivedAt = millis();
  bool receivedAnything = false;

  Serial.println("STM32 startup diagnostics:");

  while (millis() - lastReceivedAt < timeoutMs) {
    while (Serial2.available()) {
      String line = Serial2.readStringUntil('\n');
      line.trim();

      if (line.length() > 0) {
        Serial.print("STM32: ");
        Serial.println(line);
        receivedAnything = true;
      }

      lastReceivedAt = millis();
    }

    delay(10);
  }

  if (!receivedAnything) {
    Serial.println("STM32: no startup diagnostics received");
  }
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
  screenLine("SLEEP 60 MIN", 160);
  Serial.println("Going to deep sleep for 60 minutes");

  client.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
  esp_sleep_enable_ext0_wakeup(WAKE_BUTTON_PIN, 0);
  delay(100);
  esp_deep_sleep_start();
}

void requestMeasurement()
{
  Serial2.println("MEASURE");
  Serial.println("STM32 wake via UART sent");
}

// -------------------- SETUP --------------------

void setup()
{
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  printStmStartupDiagnostics();

  pinMode((uint8_t)WAKE_BUTTON_PIN, INPUT_PULLUP);

  pinMode(DIAG_LED_PIN, OUTPUT);
  digitalWrite(DIAG_LED_PIN, LOW);
  diagBlink(1);  // Wake-up indication

  /* Wake the STM32 immediately. While the ESP32 connects to Wi-Fi, the STM32
     performs the measurement. The JSON response is buffered in the ESP32 UART
     RX buffer and is consumed later by readMeasurement(). */
  requestMeasurement();

  tft.init();
  tft.setRotation(0);
  screenHeader("SMART GARDEN");

  if (!connectWiFi()) {
    diagBlink(2);
    goToSleep();
    return;
  }

  String line = readMeasurement();

  if (line.length() > 0) {
    screenHeader("DATA");
    screenLine(line, 40);

    // Do not keep an MQTT connection alive while waiting up to one minute for
    // the STM32. PubSubClient otherwise misses its keepalive window.
    client.setServer(MQTT_SERVER, MQTT_PORT);
    bool ok = false;
    if (connectMQTT()) {
      String topic = topicForPayload(line);
      ok = client.publish(topic.c_str(), line.c_str(), false);

      if (ok) {
        screenLine("MQTT SENT", 120);
        Serial.print("MQTT SENT: ");
        Serial.println(topic);
        diagBlink(1);  // Published successfully
      } else {
        screenLine("MQTT FAIL", 120);
        Serial.println("MQTT FAIL");
        diagBlink(2);  // MQTT publish failed
      }
    } else {
      screenLine("MQTT FAIL", 120);
      diagBlink(2);
    }

    // Send ACK to the STM32 regardless of the MQTT result.
    // The STM32 waits for this signal before entering STOP mode and has
    // its own 30-second ACK timeout as a fallback.
    Serial2.println("OK");
    Serial2.flush();
    delay(50);
  } else {
    screenHeader("NO DATA");
    Serial.println("No valid STM32 JSON received before timeout");
    diagBlink(3);  // No UART measurement received from STM32
    // Do not send ACK here. The STM32 either did not wake or will return to
    // STOP mode on its own after the 30-second timeout.
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
