#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define I2C_SDA_PIN 8      // must match wiring
#define I2C_SCL_PIN 9      // must match wiring
#define SLAVE_ADDR 0x08    // address of the outside ESP

// 16x2 LCD at 0x27; change if needed
LiquidCrystal_I2C lcd(0x27, 16, 2);

uint8_t motionState = 0;   // 0 = no motion, 1 = motion

void updateLCD() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("SMART DOOR SYS");

  lcd.setCursor(0, 1);
  lcd.print("MOTION: ");
  if (motionState) {
    lcd.print("DETECTED");
  } else {
    lcd.print("NONE");
  }
}

void setup() {
  Serial.begin(115200);

  // I2C master: SDA, SCL
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // LCD init
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SMART DOOR SYS");
  lcd.setCursor(0, 1);
  lcd.print("STARTING...");

  Serial.println("Inside node (I2C master, LCD) ready");
}

void loop() {
  // Ask outside ESP (slave) for 1 byte
  Wire.requestFrom(SLAVE_ADDR, (uint8_t)1);

  if (Wire.available()) {
    motionState = Wire.read();
    Serial.print("Motion from slave: ");
    Serial.println(motionState ? "DETECTED" : "NONE");
    updateLCD();

    // Here later you can add:
    // if (motionState) { trigger buzzer, start timer, etc. }
  } else {
    Serial.println("No data from slave");
  }

  delay(200);   // poll about 5 times per second
}