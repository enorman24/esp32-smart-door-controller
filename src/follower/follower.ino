#include <Wire.h>

#define ADDR 8

void onReceive(int len) {
  while (Wire.available()) {
    int command = Wire.read();
    
    if (command == 1) { //correct password

    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.onReceive(onReceive);
  Wire.begin(ADDR); 
}



void loop() {
}