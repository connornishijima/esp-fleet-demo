#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "FastLED.h"
#include <Ticker.h>
#include "FS.h"
#include <ArduinoJson.h>

String vendor_id = "1234";
String product_id = "5678";
String firmware_version = "0.0.1";
String chip_id = "123456_7890";

char* api_host = "api.espfleet.com";
String api_url = "/v1/check_in/"+vendor_id+"/"+product_id+"/"+chip_id+"/"+firmware_version;

const byte statusPin = 4;
Ticker blinker;
Ticker fadeout;
volatile byte blinkState = 1;
CRGB statusColor = (0, 0, 0);

CRGB statusLED[1];

void blinkToggle() {
  blinkState = !blinkState;
  if (blinkState == 1) {
    statusLED[0] = statusColor;
  }
  else {
    statusLED[0] = CRGB(0, 0, 0);
  }
  FastLED.show();
}

void fade() {
  byte red = statusLED[0].r;
  byte grn = statusLED[0].g;
  byte blu = statusLED[0].b;
  if (red > 0) {
    red--;
  }
  if (grn > 0) {
    grn--;
  }
  if (blu > 0) {
    blu--;
  }
  statusLED[0] = CRGB(red, grn, blu);
  FastLED.show();
  if (red == 0 && grn == 0 && blu == 0) {
    fadeout.detach();
  }
}

void statusFadeOut() {
  blinker.detach();
  fadeout.attach_ms(5, fade);
}

void statusWrite(byte r, byte g, byte b, uint16_t blinkSpeed = 0) {
  statusLED[0] = CRGB(r, g, b);
  statusColor = CRGB(r, g, b);
  FastLED.show();
  if (blinkSpeed == 0) {
    FastLED.setBrightness(255);
    blinker.detach();
  }
  else {
    blinker.attach_ms(blinkSpeed, blinkToggle);
  }
}

void fleetCheck(char* host, String url) {
  Serial.print("Connecting to "); Serial.println(host);

  // Make connection
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  // Make request
  Serial.print("Requesting URL: ");
  Serial.println(url);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(10);

  String output = "";
  Serial.println("Respond:");

  bool skip = true;
  while (client.available()) {
    String line;
    line = client.readStringUntil('\n');
    if (line == "\r") { // Used to discard HTTP GET headers
      skip = false;
    }
    if (skip == false) {
      output += line;
      Serial.println(line);
    }
  }
  Serial.println("Closing connection.");

  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(output);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  const char* server_version = root["server_version"];
  if(server_version != firmware_version){
    Serial.println("UPDATE NEEDED ------------!");
  }
}

void fleetConnect() {
  Serial.println("Booting");
  FastLED.addLeds<NEOPIXEL, statusPin>(statusLED, 1);
  statusWrite(255, 255, 0, 200);

  SPIFFS.begin();
  File f;
  char ssidBuf[64];
  char passBuf[64];
  Serial.println("Reading WiFi config from SPIFFS...");
  f = SPIFFS.open("/ssid.txt", "r");
  String ssid = f.readStringUntil('\n');
  ssid.toCharArray(ssidBuf, 64);
  f = SPIFFS.open("/pass.txt", "r");
  String pass = f.readStringUntil('\n');
  pass.toCharArray(passBuf, 64);

  statusWrite(255, 255, 0, 100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssidBuf, passBuf);
  Serial.println("Connecting to " + ssid + "...");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    statusWrite(255, 0, 0);
    Serial.println("Connection Failed! Rebooting...");
    delay(3000);
    ESP.restart();
  }
  statusWrite(0, 255, 255, 100);
  fleetCheck(api_host,api_url);
  statusWrite(0, 255, 255);
  statusFadeOut();
  otaSetup();
}

void otaSetup() {
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  fleetConnect();
}

void loop() {
  if(millis() % 5000 == 0){
    fleetCheck(api_host,api_url);
  }
  ArduinoOTA.handle();
}
