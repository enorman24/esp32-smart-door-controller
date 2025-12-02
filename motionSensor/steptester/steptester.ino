#include <ESP32Servo.h>

Servo myServo;

const int SERVO_PIN = 18;   // servo signal pin
const int PIR_PIN   = 4;    // PIR output pin

int lastState = LOW;
int currentAngle = 0;

void setup() {
  Serial.begin(115200);

  myServo.attach(SERVO_PIN, 500, 2400);
  currentAngle = 0;
  myServo.write(currentAngle);

  pinMode(PIR_PIN, INPUT);

  // Header for Serial Plotter: names of the two variables
  Serial.println("PIR Servo");
}

void loop() {
  int pirState = digitalRead(PIR_PIN);

  // Move servo instantly when state changes
  if (pirState != lastState) {
    lastState = pirState;

    if (pirState == HIGH) {
      currentAngle = 360;
      myServo.write(currentAngle);
    } else {
      currentAngle = 0;
      myServo.write(currentAngle);
    }
  }

  // Print values for Serial Plotter (two lines on one row)
  // Format: PIR Servo
  // Serial.print(pirState);      // 0 or 1
  // Serial.print(" ");
  // Serial.println(currentAngle); // 0 or 90

  //delay(50);  // update ~20 times per second
}




