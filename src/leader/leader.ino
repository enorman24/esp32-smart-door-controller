/**
 * @file leader.ino
 * @author Elliot Norman
 * @author Melih Boyun
 * @date 2025-12-02
 * @brief Leader ESP32 smart door controller.
 *
 * This file implements the "leader" ESP32 node for a smart door system.
 * It:
 *  - Manages the LCD user interface.
 *  - Receives password input over serial and BLE.
 *  - Verifies passwords and commands the follower ESP32 over I2C.
 *  - Uses a hardware timer and FreeRTOS queue to automatically re-lock
 *    the door after a fixed interval.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h" //for tasks
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <LiquidCrystal_I2C.h> //for using the lcd display
#include <Wire.h>              // for using 2 esp32s
#include <string.h>
#define I2C_SDA 8
#define I2C_SCL 9

#define FOLLOWER_ADDR 8
#define MAX_PASS_LEN 20
//================ BLE STUFF ======================//
#define SERVICE_UUID "56be7035-a164-4929-854d-ab699176f9fd"
#define CHARACTERISTIC_UUID "8e4b2914-ba59-432f-a51b-b66dee7838b7"
//================ Password ==================//
const String CORRECT_PASSWORD = "12345";

//===========HARDWARE TIMER===========//
hw_timer_t *doorTimer = NULL;
#define PRESCALER 80
#define UNLOCK_TIME_US 20000000
//===========QUEUE==============//

/**
 * @enum InputSource
 * @brief Identifies which subsystem produced a message for the system queue.
 *
 * Used by the manager task to dispatch messages based on their origin.
 */
enum InputSource {
  S_SERIAL,    /**< Message came from serial input. */
  S_BLE,       /**< Message came from BLE input. */
  S_LOCK_TIMER /**< Message came from the hardware auto-lock timer ISR. */
};

/**
 * @struct MessageForQueue
 * @brief Message object passed between tasks and ISR via the FreeRTOS queue.
 *
 * The same struct is used for:
 *  - Password attempts from serial and BLE.
 *  - Control messages from the lock timer ISR.
 */
typedef struct {
  char text[MAX_PASS_LEN];
  InputSource source;
} MessageForQueue;

/** @brief Global handle for the system message queue. */
QueueHandle_t xSystemQueue;
//================ LCD STUFF ======================//

/**
 * @brief Display "Unlocked" state on the LCD.
 *
 * Clears the LCD and shows a status message when the door is unlocked.
 */
LiquidCrystal_I2C lcd(0x27, 16, 2);

void lcd_correct() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Unlocked");
}

/**
 * @brief Display "Wrong Password" message on the LCD.
 *
 * Used after an incorrect password attempt.
 */
void lcd_incorrect() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wrong Password");
}

/**
 * @brief Display "LOCKED" state on the LCD.
 *
 * Called when the door transitions to or remains in the locked state.
 */
void lcd_locked() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LOCKED");
}
//================ PASSWORD CONTROL =====================//

/**
 * @brief Handle correct password entry.
 *
 * This function:
 *  - Updates the LCD to show the unlocked state.
 *  - Sends an "unlock" command (1) to the follower ESP32 over I2C.
 *  - Starts the hardware timer that will re-lock the door after
 *    @c UNLOCK_TIME_US microseconds.
 */
void passwd_correct() {
  lcd_correct();                         // display on lcd
  Wire.beginTransmission(FOLLOWER_ADDR); // send message to esp32 #2
  Wire.write(1);
  Wire.endTransmission();
  timerWrite(doorTimer, 0);
  timerStart(doorTimer); // start timer so the door
  // is closed after a set amount of time
}

/**
 * @brief Handle incorrect password entry.
 *
 * Only updates the LCD to show an error message. The manager task
 * is responsible for any additional delay and re-lock display logic.
 */
void passwd_incorrect() { lcd_incorrect(); }

/**
 * @brief Lock the door and update status.
 *
 * Sends a "lock" command (0) to the follower ESP32 and sets the LCD
 * to show the locked state. This is used at startup and when the
 * auto-lock timer expires.
 */
void lock_door() {
  Wire.beginTransmission(FOLLOWER_ADDR);
  Wire.write(0);
  Wire.endTransmission();
  lcd_locked();
}
//================ BLE HANDER ===================//

/**
 * @class MyCallbacks
 * @brief BLE characteristic callback handler for incoming password data.
 *
 * When the central writes to the BLE characteristic, this callback:
 *  - Reads the incoming string.
 *  - Truncates it safely to @c MAX_PASS_LEN - 1 characters.
 *  - Packages it into a @ref MessageForQueue with source @c S_BLE.
 *  - Sends the message to @ref xSystemQueue for processing by the manager task.
 */
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    String rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      MessageForQueue msg;
      msg.source = S_BLE;
      size_t len = rxValue.length();
      if (len >= MAX_PASS_LEN) {
        len = MAX_PASS_LEN - 1;
      }
      rxValue.toCharArray(msg.text, len + 1);
      xQueueSend(xSystemQueue, &msg, 0);
    }
  }
};
//================TIMER ISR ====================//

/**
 * @brief Hardware timer ISR for automatic door locking.
 *
 * This ISR is triggered when the door auto-lock timer expires.
 * It:
 *  - Creates a @ref MessageForQueue with source @c S_LOCK_TIMER.
 *  - Enqueues the message using @c xQueueSendFromISR so the manager task
 *    can safely perform the lock operation in task context.
 *
 * @note Runs in interrupt context; work is intentionally minimal and
 *       deferred to @ref taskSystemManager via the queue.
 */
void IRAM_ATTR onDoorTimerExpire() {
  MessageForQueue msg;
  msg.source = S_LOCK_TIMER;
  strcpy(msg.text, "LOCK"); // doesn't matter

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(xSystemQueue, &msg, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) { // guard for if there is a higher priority
                                  // task
    portYIELD_FROM_ISR();
  }
}
//================== INPUT =====================//

/**
 * @brief FreeRTOS task to read password input from the serial interface.
 *
 * The task:
 *  - Accumulates characters into a temporary @c String until a newline is seen.
 *  - Trims whitespace and ignores empty lines.
 *  - Converts the string into a @ref MessageForQueue with source @c S_SERIAL.
 *  - Sends the message to @ref xSystemQueue for verification.
 *
 * @param pvParameters Unused task parameter (required by FreeRTOS signature).
 */
void taskSerialInput(void *pvParameters) {
  String curr = "";
  while (1) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        curr.trim();
        if (curr.length() > 0) {
          MessageForQueue msg;
          msg.source = S_SERIAL;
          curr.toCharArray(msg.text, MAX_PASS_LEN); // queue holds char array
          xQueueSend(xSystemQueue, &msg, 10);
        }
        curr = "";
      } else if (c != '\r') {
        if (curr.length() < MAX_PASS_LEN - 1) {
          curr += c;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
// ================= MANAGER TASK (CONSUMER) ================== //

/**
 * @brief Main system manager task that consumes messages from the queue.
 *
 * Responsibilities:
 *  - Initially locks the door at startup.
 *  - Blocks on @c xQueueReceive waiting for messages from:
 *      - @ref taskSerialInput (@c S_SERIAL)
 *      - @ref MyCallbacks::onWrite (@c S_BLE)
 *      - @ref onDoorTimerExpire (@c S_LOCK_TIMER)
 *  - On @c S_LOCK_TIMER: locks the door and stops the auto-lock timer.
 *  - On @c S_SERIAL or @c S_BLE: compares the received password text to
 *    @ref CORRECT_PASSWORD and calls @ref passwd_correct or
 *    @ref passwd_incorrect accordingly.
 *
 * @param pvParameters Unused task parameter (required by FreeRTOS signature).
 */
void taskSystemManager(void *pvParameters) {
  MessageForQueue msg;
  lock_door(); // initially locked
  while (1) {
    if (xQueueReceive(xSystemQueue, &msg, portMAX_DELAY) == pdPASS) {
      if (msg.source == S_LOCK_TIMER) {
        lock_door();
        timerStop(doorTimer);
      } else if (msg.source == S_BLE || msg.source == S_SERIAL) {
        // compares strings char by char -> important because one is a string
        // tand he other is a character array
        if (strcmp(msg.text, CORRECT_PASSWORD.c_str()) == 0) {
          passwd_correct();
        } else {
          passwd_incorrect();
          vTaskDelay(pdMS_TO_TICKS(3000));
          lcd_locked();
        }
      }
    }
  }
}

/**
 * @brief Arduino setup function.
 *
 * Initializes:
 *  - Serial console for debugging.
 *  - LCD interface and initial display.
 *  - I2C bus to talk to the follower ESP32.
 *  - Hardware timer for auto-lock functionality.
 *  - FreeRTOS system message queue.
 *  - BLE device, service, characteristic, and advertising.
 *  - FreeRTOS tasks @ref taskSerialInput and @ref taskSystemManager,
 *    pinned to separate cores.
 */
void setup() {
  Serial.begin(115200);

  //================ LCD STUFF ======================//
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  //================= 2 ESPS ======================//
  Wire.begin(I2C_SDA, I2C_SCL);
  //================= TIMER ======================//
  doorTimer = timerBegin(1000000);
  timerAttachInterrupt(doorTimer, &onDoorTimerExpire);
  timerAlarm(doorTimer, UNLOCK_TIME_US, false, 0);
  timerStop(doorTimer);
  //================= CREATE QUEUES ======================//
  xSystemQueue = xQueueCreate(10, sizeof(MessageForQueue));
  if (xSystemQueue == NULL) {
    Serial.println("Error creating the queue");
  }
  //================ BLE STUFF ======================//
  BLEDevice::init("MelihElliotESP");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();

  //================ FREERTOS TASKS ===============//
  xTaskCreatePinnedToCore(taskSerialInput, "SerialTask", 4096, NULL, 1, NULL,
                          0);

  xTaskCreatePinnedToCore(taskSystemManager, "ManagerTask", 4096, NULL, 1, NULL,
                          1);
  //================ FREERTOS TASKS ===============//
}

/**
 * @brief Arduino main loop.
 *
 * Left empty because all behavior is implemented using FreeRTOS tasks
 * and interrupt handlers.
 */
void loop() {}
