#include <Wire.h>
#include <ESP32Servo.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" //for tasks


#define SERVO_PIN 18 //motor pin
#define PIR_PIN 4 // motion sensor pin
#define ADDR 8 //address in I2C connection
#define BUZZER_PIN 19 // buzzer pin

Servo myServo;
hw_timer_t *debounceTimer = NULL;
volatile bool pirState = false;

//===========QUEUES==============//
QueueHandle_t xI2cQueue;
QueueHandle_t xBuzzerQueue;

//===========QUEUES==============//


//===========ISRs=============//
void IRAM_ATTR onTimerDebounce() {
  pirState = digitalRead(PIR_PIN);
}

void IRAM_ATTR onPIRChange() {
  timerWrite(debounceTimer, 0);
  timerStart(debounceTimer);
}

void onReceive(int len) {
  while (Wire.available()) {
    int command = Wire.read();
    xQueueSendFromISR(xI2cQueue, &command, NULL);
  }
}
//===============ISRs================//

//===========BUZZER TASK==============//
void TaskBuzzer(void *parameter) {
  int msg;
  while (1) {
    if (xQueueReceive(xBuzzerQueue, &msg, portMAX_DELAY)) { //true if it receives
      digitalWrite(BUZZER_PIN, HIGH); //buzzer high for one second
      vTaskDelay(1000 / portTICK_PERIOD_MS); // Non-blocking delay
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}
//===========BUZZER TASK==============//

//===========CONTROLLER TASK==============//
void TaskController(void *parameter) {
  int command;
  int currentAngle = 0;
  while(1) {
    if (xQueueReceive(xI2cQueue, &command, portMAX_DELAY)) {
      if (command == 1) { 
        currentAngle = 360; 
        myServo.write(currentAngle);
      } else {
        if (pirState == HIGH) {
          int trigger = 1; 
          xQueueSend(xBuzzerQueue, &trigger, portMAX_DELAY); //pass trigger mem
          //addr not value
          currentAngle = 0;
          myServo.write(currentAngle);
        }
      }
    }
  }
}

//===========CONTROLLER TASK==============//


void setup() {
  Serial.begin(115200);

  //I2C
  Wire.onReceive(onReceive);
  Wire.begin(ADDR); 
  //I2C

  //IO PINS
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  //IO PINS

  //Servo and Sensor setup
  myServo.attach(SERVO_PIN, 500, 2400);
  //Servo and Sensor setup

  //QUEUE SETUP
  xI2cQueue = xQueueCreate(10, sizeof(int)); 
  xBuzzerQueue = xQueueCreate(5, sizeof(int));
  //QUEUE SETUP

  //TIMER
  debounceTimer = timerBegin(1000000);
  timerStop(debounceTimer);
  timerAttachInterrupt(debounceTimer, &onTimerDebounce);
  timerAlarm(debounceTimer, 50000, false, 0);
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), onPIRChange, CHANGE); //digital pin to interrupt
  //translates the pin to the internal interupt 'location'
  //TIMER

  //CREATE TASKS
  xTaskCreate(TaskController, "Logic Task", 4096, NULL, 1, NULL);
  xTaskCreate(TaskBuzzer, "Buzzer Task", 2048, NULL, 1, NULL);
  //CREATE TASKS
}



void loop() {
}