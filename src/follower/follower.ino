#include <Wire.h>

//unlock time
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"


//======Timers======//
hw_timer_t * unlock_timer = NULL; //will keep track of how long since the door has been unlocked
hw_timer_t * alarm_timer = NULL; //keeps track of how long the alarm has been going off for



#define ADDR 8
#define PIR_PIN 2     
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9



//==============MOTION DETECTOR=======================//
void TaskDetectMotion(void) { //CHANGE TO INTERUPT
  int motion = digitalRead(PIR_PIN);
}



//=================MOTION DETECTOR=======================//

//=================INPUTS FROM LEADER====================//
void TaskEspConnection(void * ___) {
  //determine if this should be a task or an interrupt
}


void onReceive(int len) { //use a semaphone?
  while (Wire.available()) {
    int command = Wire.read();
    if (command == 1) { //correct password -> door is unlocked for 30 seconds
    }
  }
}

//===================== MOTOR =====================//


//===================== MOTOR =====================//


void setup() {
  Serial.begin(115200);
  Wire.onReceive(onReceive);
  Wire.begin(ADDR); 

  pinMode(PIR_PIN, INPUT); //sensor

  //==============Timer initialize==========//
  //keeps track of the 30 seconds since the door was unlocked

  
  //==============Timer initialize==========//


}



void loop() {
}