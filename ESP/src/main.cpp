#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TFT_eSPI.h>

const char* WIFI_SSID = "FRITZ!Box 7530 TX";
const char* WIFI_PASS = "91323847553668447857";
const char* SERVER_URL = "https://mimozalab.com/api/measurements";

TFT_eSPI tft = TFT_eSPI();


// ---------- DISPLAY HELPERS ----------
void screenHeader(const char* title)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0,0);
  tft.println(title);
  tft.drawFastHLine(0,20,240,TFT_GREEN);
  tft.setTextSize(2);
}

void screenLine(String txt, int y)
{
  tft.setCursor(0,y);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.println(txt);
}


// ---------- HTTP POST ----------
int sendMeasurement(float temperature, float humidity)
{
  String json =
    "{"
    "\"sensorId\":1,"
    "\"temperature\":" + String(temperature, 1) + ","
    "\"humidity\":" + String(humidity, 1) + ","
    "\"timestamp\":\"2026-01-06T18:00:00\""
    "}";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, SERVER_URL);
  http.addHeader("Content-Type","application/json");

  int code = http.POST(json);

  http.end();
  return code;
}


// ---------- GET CHECK ----------
int checkServerGet()
{
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, SERVER_URL);
  int code = http.GET();
  http.end();

  return code;
}


// ---------- SETUP ----------
void setup()
{
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);
  
  // Turn on backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  screenHeader("ESP Monitor");

  screenLine("Connecting WiFi...",30);

  WiFi.begin(WIFI_SSID,WIFI_PASS);

  while(WiFi.status()!=WL_CONNECTED)
  {
    delay(400);
    Serial.print(".");
  }

  screenLine("WiFi OK",60);
  screenLine(WiFi.localIP().toString(),90);

  delay(2000);
}


// ---------- LOOP ----------
void loop()
{
  if(WiFi.status()!=WL_CONNECTED)
  {
    screenHeader("ERROR");
    screenLine("WiFi Lost",60);
    delay(2000);
    return;
  }

  float temperature = 23.6;
  float humidity = 48.2;

  // --- GET ---
  int getCode = checkServerGet();

  screenHeader("SERVER CHECK");
  screenLine("GET code:",40);
  screenLine(String(getCode),70);

  delay(1500);

  // --- POST ---
  int postCode = sendMeasurement(temperature, humidity);

  screenHeader("POST RESULT");

  screenLine("Temp: "+String(temperature),40);
  screenLine("Hum : "+String(humidity),70);

  screenLine("HTTP:",120);
  screenLine(String(postCode),150);

  delay(5000);
}