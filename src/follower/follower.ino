/**
 * @file follower.ino
 * @author Elliot Norman
 * @author Melih Boyun
 * @date 2025-12-02
 * @brief Follower ESP32 smart door controller.
 *
 * This file implements the "follower" ESP32, which:
 *  - Receives lock/unlock commands from the leader over I2C.
 *  - Drives a servo to physically open/close the door.
 *  - Monitors a PIR motion sensor with hardware-timer-based debouncing.
 *  - Triggers a buzzer alarm when motion is detected while locked.
 *
 * All real-time behavior is implemented using FreeRTOS tasks, ISRs, and queues.
 */

#include <Wire.h>
#include <ESP32Servo.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" //for tasks


#define SERVO_PIN 18 //motor pin
#define PIR_PIN 4 // motion sensor pin
#define ADDR 8 //address in I2C connection
#define BUZZER_PIN 19 // buzzer pin

const uint32_t LOW_FREQ    = 50;
const uint32_t MEDIUM_FREQ = 500;
const uint32_t HIGH_FREQ   = 10000;

const uint8_t  RESOLUTION     = 12;
const uint16_t MAX_DUTY_CYCLE = (1 << RESOLUTION) - 1;
const uint16_t DUTY_CYCLE     = (MAX_DUTY_CYCLE >> 1);


/** @brief Servo instance that drives the door mechanism. */
Servo myServo;

/** @brief Hardware timer used to debounce the PIR motion sensor. */
hw_timer_t *debounceTimer = NULL;

/**
 * @brief Latched PIR sensor state.
 *
 * Updated by the debounce timer ISR and consumed by the controller task
 * to decide whether to trigger the buzzer when the door is commanded to lock.
 */
volatile bool pirState = false;

//===========QUEUES==============//

/**
 * @brief Queue for commands received over I2C.
 *
 * Each item is an @c int command from the leader:
 *  - 1: unlock (open door / set servo to open position)
 *  - other: lock request, which may trigger the buzzer if motion is detected.
 */
QueueHandle_t xI2cQueue;

/**
 * @brief Queue used to trigger the buzzer task.
 *
 * Any value pushed to this queue causes @ref TaskBuzzer to sound the buzzer
 * for a fixed duration.
 */
QueueHandle_t xBuzzerQueue;

//===========QUEUES==============//


//===========ISRs=============//

/**
 * @brief Debounce timer ISR for the PIR sensor.
 *
 * This ISR runs after a short debounce interval and samples the PIR pin.
 * The result is stored in @ref pirState, which is then used by the controller
 * task to decide whether motion is present.
 */
void IRAM_ATTR onTimerDebounce() {
  timerStop(debounceTimer);
  pirState = digitalRead(PIR_PIN);
}

/**
 * @brief PIR GPIO change ISR.
 *
 * Triggered on any change of the PIR input. Instead of reading the pin
 * immediately (which may be noisy), this ISR restarts the debounce timer.
 * The actual sampling of the PIR pin happens in @ref onTimerDebounce.
 */
void IRAM_ATTR onPIRChange() {
  timerWrite(debounceTimer, 0);
  timerStart(debounceTimer);
}

/**
 * @brief I2C receive callback.
 *
 * Called by the Wire library when the follower receives data from the leader.
 * It:
 *  - Reads incoming bytes as @c int commands.
 *  - Sends each command into @ref xI2cQueue using @c xQueueSendFromISR so the
 *    controller task can handle them in task context.
 *
 * @param len Number of bytes available in the receive buffer.
 */
void onReceive(int len) {
  while (Wire.available()) {
    int command = Wire.read();
    xQueueSendFromISR(xI2cQueue, &command, NULL);
  }
}
//===============ISRs================//

//===========BUZZER TASK==============//

/**
 * @brief FreeRTOS task that controls the buzzer output.
 *
 * The task:
 *  - Blocks on @ref xBuzzerQueue waiting for any message.
 *  - When a message is received, drives @ref BUZZER_PIN high for 1 second.
 *  - Turns the buzzer off and waits for the next trigger.
 *
 * @param parameter Unused task parameter (required by FreeRTOS).
 */
void TaskBuzzer(void *parameter) {
  int msg;
  while (1) {
    if (xQueueReceive(xBuzzerQueue, &msg, portMAX_DELAY)) { //true if it receives
      ledcWrite(BUZZER_PIN, DUTY_CYCLE); 
      ledcWriteTone(BUZZER_PIN, LOW_FREQ); 
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      ledcWriteTone(BUZZER_PIN, MEDIUM_FREQ);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      ledcWriteTone(BUZZER_PIN, HIGH_FREQ);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      ledcWrite(BUZZER_PIN, 0); 
    }
  }
}
//===========BUZZER TASK==============//

//===========CONTROLLER TASK==============//

/**
 * @brief Main controller task for servo and motion-locked behavior.
 *
 * Responsibilities:
 *  - Waits for lock/unlock commands from @ref xI2cQueue:
 *      - command == 1: unlock — sets @c currentAngle to 360 and writes it
 *        to the servo (door open).
 *      - any other command: lock request — if @ref pirState is HIGH (motion
 *        detected), it:
 *          - sends a trigger to @ref xBuzzerQueue to sound the alarm.
 *          - sets @c currentAngle to 0 and writes it to the servo (door closed).
 *  - Inserts a small periodic delay to yield CPU time.
 *
 * @param parameter Unused task parameter (required by FreeRTOS).
 */
void TaskController(void *parameter) {
  int command = 0;
  int currentAngle = 0;
  int curr_state = 0;
  bool open = false;
  bool alarmTriggered = false; 

  while (1) {
    if (xQueueReceive(xI2cQueue, &command, 0) == pdTRUE) {
      if (command == 1) { 
        curr_state = 1; 
      } else if (command == 0) {
        if(open) {
          currentAngle = 0;
          myServo.write(currentAngle); 
          open = false;
        }
        curr_state = 0;
      }   
    }
    if (pirState && curr_state == 1) {
        currentAngle = 360;
        myServo.write(currentAngle);
        open = true;
    }
    if (pirState) {
      if (curr_state == 0 && !alarmTriggered) {
        int trigger = 1; 
        if(uxQueueSpacesAvailable(xBuzzerQueue) > 0) {
           xQueueSend(xBuzzerQueue, &trigger, 0);
           alarmTriggered = true;
        }
      }
    } else {
      alarmTriggered = false;
    }
    vTaskDelay(pdMS_TO_TICKS(8));
  }
}


//===========CONTROLLER TASK==============//


/**
 * @brief Arduino setup function.
 *
 * Initializes:
 *  - Serial port for debugging.
 *  - I2C in follower mode at address @ref ADDR and registers @ref onReceive.
 *  - GPIO directions for PIR sensor and buzzer.
 *  - Servo attachment and limits.
 *  - FreeRTOS queues @ref xI2cQueue and @ref xBuzzerQueue.
 *  - Hardware debounce timer and PIR GPIO interrupt @ref onPIRChange.
 *  - FreeRTOS tasks @ref TaskController and @ref TaskBuzzer pinned to
 *    separate cores.
 */
void setup() {
  Serial.begin(115200);

  //I2C
  Wire.onReceive(onReceive);
  Wire.begin(ADDR); 
  //I2C

  //IO PINS
  pinMode(PIR_PIN, INPUT);
  ledcAttach(BUZZER_PIN, LOW_FREQ, RESOLUTION);
  ledcWrite(BUZZER_PIN, 0);                    
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
  //TIMER

  //================ FREERTOS TASKS ===============//
  xTaskCreatePinnedToCore(
    TaskController,
    "Logic Task",
    4096,
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    TaskBuzzer,
    "Buzzer Task",
    4096,
    NULL,
    1,
    NULL,
    1
  );
  //================ FREERTOS TASKS ===============//
}

/**
 * @brief Arduino main loop.
 *
 * Intentionally empty; all work is handled by FreeRTOS tasks and ISRs.
 */
void loop() {
}
