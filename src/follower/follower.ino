#include <Wire.h>
#include <ESP32Servo.h>

Servo myServo;

#define SERVO_PIN 18   
#define PIR_PIN   4    
#define ADDR 8
#define BUZZER_PIN 19

int lastState = LOW;
int pirState = digitalRead(PIR_PIN);
int currentAngle = 0;

void moveServo_withSensor(int pirState, int lastState, int currentAngle) {
// Move servo instantly when state changes
  if (pirState != lastState) {
    lastState = pirState;
    if (pirState == HIGH) {
      currentAngle = 360;
      myServo.write(currentAngle);
    }else{
      currentAngle = 0;
      myServo.write(currentAngle);
    }
  }
}

void onReceive(int len, int pirState, int lastState) {
  while (Wire.available()) {
    int command = Wire.read();
    
    if (command == 1) { //correct password
      moveServo_withSensor(pirState, lastState, currentAngle);
    }else{
      if(pirState == HIGH){
        digitalWrite(BUZZER_PIN, HIGH); 
        delay(1000);                   // Buzzer on for 1 second
        digitalWrite(BUZZER_PIN, LOW);  
      }
    }
  }
}


void setup() {
  Serial.begin(115200);
  Wire.onReceive(onReceive);
  Wire.begin(ADDR); 
  pinMode(BUZZER_PIN, OUTPUT);

  //Servo and Sensor setup
  myServo.attach(SERVO_PIN, 500, 2400);
  currentAngle = 0;
  myServo.write(currentAngle);
  pinMode(PIR_PIN, INPUT);
}



void loop() {
}