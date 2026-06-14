#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <PubSubClient.h>

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

// -------------------- SETUP --------------------

void setup()
{
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  tft.init();
  tft.setRotation(1);
  screenHeader("SMART GARDEN");

  connectWiFi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  connectMQTT();
}

// -------------------- LOOP --------------------

void loop()
{
  if (!client.connected()) {
    connectMQTT();
  }

  client.loop();

  if (Serial2.available()) {

    String line = Serial2.readStringUntil('\n');
    line.trim();

    Serial.print("UART: ");
    Serial.println(line);

    if (line.length() > 5) {

      screenHeader("DATA");
      screenLine(line, 40);

      String topic = "smartgarden/" + String(DEVICE_ID) + "/data";

      bool ok = client.publish(topic.c_str(), line.c_str(), false);

      if (ok) {
        screenLine("MQTT SENT", 120);
        Serial.println("MQTT SENT");
      } else {
        screenLine("MQTT FAIL", 160);
        Serial.println("MQTT FAIL");
      }
    }
  }
}
