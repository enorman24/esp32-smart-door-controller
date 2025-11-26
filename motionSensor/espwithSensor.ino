#include <Wire.h>

#define PIR_PIN 2          // PIR sensor output pin
#define I2C_SDA_PIN 8      // change to match your wiring
#define I2C_SCL_PIN 9      // change to match your wiring
#define SLAVE_ADDR 0x08    // I2C address for this ESP (outside node)

volatile uint8_t motionState = 0;  // 0 = no motion, 1 = motion

void onRequestHandler() {
  // Master is asking for the latest motion state
  Wire.write(motionState);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);

  // I2C slave: address, SDA, SCL
  Wire.begin(SLAVE_ADDR, I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.onRequest(onRequestHandler);

  Serial.println("Outside node (I2C slave, PIR) ready");
}

void loop() {
  int pirValue = digitalRead(PIR_PIN);
  motionState = (pirValue == HIGH) ? 1 : 0;

  Serial.print("PIR: ");
  Serial.println(motionState ? "MOTION" : "NO MOTION");

  delay(100);  // small delay; I2C will still work fine
}