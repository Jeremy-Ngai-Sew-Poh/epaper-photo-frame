#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>

#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_ED2208.h"
#include "panel/Panel_EPaper.h"
#include "secrets.h"

#define VOLTAGE_PIN A0
#define ADC_ENABLE_PIN A5

#define uS_TO_M_FACTOR 60000000ULL
#define sleepTime 30

Seeed_GFX display;
Preferences preferences;

const char* GH_OWNER = "Jeremy-Ngai-Sew-Poh";
const char* GH_REPO = "epaper_photo_list";
const char* BIN_PATH = "images/photo_02.bin";

void setup() {
  Serial.begin(115200);

  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 2500)) {
    delay(10);
  }
  delay(200);

  checkBattery();

  preferences.begin("images", false);
  unsigned long bootCount = preferences.getInt("boot", 0);

  bootCount++;

  preferences.putInt("boot", bootCount);
  preferences.end();

  Serial.print("Boot #");
  Serial.println(bootCount);

  //initialise epaper
  if (!display.begin<Board_XIAO_ePaper_EE04, Config_Seeed_ePaper_7inch3_Colorful_ED2208>()) {
    Serial.println("Displat init failed");
    Serial.println(display.lastResult().message);
    return;
  }

  //reserve psram for array
  size_t bufSize = ((size_t)display.width() * (size_t)display.height()) / 2; //192,000 bytes
  uint8_t* buf = (uint8_t*)ps_malloc(bufSize);
  if (!buf) {
    Serial.println("PSRAM failed.");
    return;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected.");
  

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String imgUrl = "https://api.github.com/repos/" + String(GH_OWNER) + "/" + String(GH_REPO) + "/contents/" + String(BIN_PATH) + "?ref=main";

  http.begin(client, imgUrl);
  http.addHeader("Authorization", String("Bearer ") + GITHUB_PAT);
  http.addHeader("Accept", "application/vnd.github.raw");
  http.addHeader("User-Agent", "ESP32");

  int code = http.GET();
  Serial.printf("HTTP code: %d . Size: %d\n", code, http.getSize());

  if (code == 200) {
    WiFiClient* stream = http.getStreamPtr();
    size_t got = 0;
    while (http.connected() && got < bufSize) {
      if (stream->available()) {
        got += stream->readBytes(buf + got, bufSize - got);
      } else {
        delay(1);
      }
    }
    Serial.printf("Downloaded %u bytes\n", (unsigned)got);

    if (got == bufSize) {
      display.fillScreen(TFT_WHITE);
      display.pushImage4BPP(0, 0, display.width(), display.height(), buf, true);
      display.refresh();
    }
  } else {
    Serial.println(http.getString());
  }
  
  http.end();
  free(buf);

  enterDeepSleep(sleepTime);
}

void loop() {
  // put your main code here, to run repeatedly:
}

void checkBattery() {
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(ADC_ENABLE_PIN, OUTPUT);
  digitalWrite(ADC_ENABLE_PIN, HIGH);

  delay(10);

  analogReadResolution(12);
  int adcValue = analogRead(VOLTAGE_PIN);
  float voltage = (adcValue / 4096.0) * 7.16;
  int btyLevel = (int)(((voltage - 3.2) / (4.2 - 3.2)) * 100.0);
  btyLevel = constrain(btyLevel, 0, 100);

  Serial.print("ADC: ");
  Serial.print(adcValue);
  Serial.print(" Voltage: ");
  Serial.print(voltage);
  Serial.print(" Level: ");
  Serial.println(btyLevel);

  digitalWrite(ADC_ENABLE_PIN, LOW);

  delay(100);
}

void enterDeepSleep(int sleepTime) {
  esp_sleep_enable_timer_wakeup(sleepTime * uS_TO_M_FACTOR);
  esp_deep_sleep_start();
}
