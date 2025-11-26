#include <esp_now.h>
#include <WiFi.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// 16x2 LCD at address 0x27 (change if your LCD is different)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Last motion state received
int lastMotion = 0;
String linkStatus = "WAITING";

void updateLCD() {
  lcd.clear();

  // Line 1: motion status
  lcd.setCursor(0, 0);
  lcd.print("MOTION: ");
  if (lastMotion == HIGH) {
    lcd.print("DETECTED");
  } else {
    lcd.print("NONE");
  }

  // Line 2: link status
  lcd.setCursor(0, 1);
  lcd.print("LINK: ");
  lcd.print(linkStatus);
}

// Callback when ESP-NOW data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(int)) {
    memcpy(&lastMotion, incomingData, sizeof(int));
    linkStatus = "OK";
  } else {
    linkStatus = "ERR";
  }

  Serial.print("Motion: ");
  Serial.println(lastMotion == HIGH ? "Detected" : "Not Detected");

  updateLCD();

  // 👉 later you can add logic here:
  // if (lastMotion == HIGH) { trigger buzzer / start timer / etc. }
}

void setup() {
  Serial.begin(115200);

  // LCD init
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SMART DOOR SYS");
  lcd.setCursor(0, 1);
  lcd.print("WAITING MOTION");

  // Wi-Fi in station mode
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register receive callback
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Inside node (LCD receiver) ready");
}

void loop() {
  // For now nothing here; everything happens on receive
}
