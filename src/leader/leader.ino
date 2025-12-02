/*
PURPOSE
- Control the display
- Watch for user input (BLE/Serial)
- Tell ESP32 #2 to open/close door
- Listen for inputs from ESP32 for sensors
*/


/*ADDITIONS
QUEUE FOR INPUTS
separate serial in and ble in tasks
task for password manager/controller (controls lcd and output to other esp32) -> it also uses a timer
*/

#include <Arduino.h>
#include <LiquidCrystal_I2C.h> //for using the lcd display
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" //for tasks
#include <Wire.h> // for using 2 esp32s
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h> 
#include <string.h>


#define FOLLOWER_ADDR 8
//================ Password ==================//
#define MAX_PASS_LEN 20


const String CORRECT_PASSWORD = "12345"; 

//===========HARDWARE TIMER===========//
hw_timer_t * doorTimer = NULL;
#define PRESCALER 80
#define UNLOCK_TIME_US 20000000
//===========HARDWARE TIMER===========//

//===========QUEUE==============//
enum InputSource { 
    SOURCE_SERIAL, 
    SOURCE_BLE, 
    SOURCE_TIMER_AUTO_LOCK
};

typedef struct {
    char text[MAX_PASS_LEN]; // Holds password OR status message
    InputSource source;
} SystemMessage;

QueueHandle_t xSystemQueue;
//===========QUEUE==============//


//================ BLE STUFF ======================//
#define SERVICE_UUID        "56be7035-a164-4929-854d-ab699176f9fd"
#define CHARACTERISTIC_UUID "8e4b2914-ba59-432f-a51b-b66dee7838b7"

//================ BLE STUFF ======================//

//================ LCD STUFF ======================//
LiquidCrystal_I2C lcd(0x27, 16, 2);

void lcd_correct(){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Unlocked");
    
}

void lcd_incorrect(){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Wrong Password");
}

void lcd_locked(){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("LOCKED");
}
//================ LCD STUFF ======================//

//================ PASSWORD CONTROL =====================//

void passwd_correct(){
    lcd_correct(); //display on lcd
    Wire.beginTransmission(FOLLOWER_ADDR); //send message to esp32 #2
    Wire.write(1);
    Wire.endTransmission();
    timerWrite(doorTimer, 0); 
    timerStart(doorTimer);

}

void passwd_incorrect(){
    lcd_incorrect();
}

void lock_door(){
    Wire.beginTransmission(FOLLOWER_ADDR);
    Wire.write(0); // 0 = Lock
    Wire.endTransmission();
    lcd_locked();
}
//================ PASSWORD C0NTROL =====================//

//================ BLE HANDER ===================//
class MyCallbacks: public BLECharacteristicCallbacks {
   void onWrite(BLECharacteristic *pCharacteristic) override {
     String rxValue = pCharacteristic->getValue();
     if (rxValue.length() > 0) {
        SystemMessage msg;
        msg.source = SOURCE_BLE;
        size_t len = rxValue.length();
        if (len >= MAX_PASS_LEN) {
            len = MAX_PASS_LEN - 1;
        }

        rxValue.toCharArray(msg.text, len + 1);  // copies and null-terminates

        xQueueSend(xSystemQueue, &msg, 0);

     }
   }
};
//================ BLE HANDER ===================//

//================TIMER ISR ====================//
void IRAM_ATTR onDoorTimerExpire() {
    SystemMessage msg;
    msg.source = SOURCE_TIMER_AUTO_LOCK;
    strcpy(msg.text, "TIMEOUT"); 
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(xSystemQueue, &msg, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}
//================TIMER ISR ====================//


//================== INPUT =====================//
void taskSerialInput(void *pvParameters) {
    static String curr = "";

    while(1) {
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n') {
                curr.trim();
                if (curr.length() > 0) {
                    SystemMessage msg;
                    msg.source = SOURCE_SERIAL;
                    curr.toCharArray(msg.text, MAX_PASS_LEN);

                    xQueueSend(xSystemQueue, &msg, 10);
                }
                curr = "";
            }
            else if (c != '\r') {
                if (curr.length() < MAX_PASS_LEN - 1) {
                    curr += c;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield
    }
}

// ================= MANAGER TASK (CONSUMER) ================== //
void taskSystemManager(void *pvParameters) {
    SystemMessage msg;

    // Start locked
    lock_door();

    for (;;) {
        // Block here until we get ANY event
        if (xQueueReceive(xSystemQueue, &msg, portMAX_DELAY) != pdPASS) {
            continue;
        }

        Serial.print("Event Source: ");
        Serial.println(msg.source);

        // --- AUTO-LOCK TIMER EXPIRED ---
        if (msg.source == SOURCE_TIMER_AUTO_LOCK) {
            Serial.println("Timer Expired. Locking Door.");
            lock_door();
            // Stop the timer so it doesn't keep firing
            timerStop(doorTimer);
            continue;
        }

        // --- PASSWORD INPUT (BLE or Serial) ---
        if (msg.source == SOURCE_BLE || msg.source == SOURCE_SERIAL) {
            Serial.print("Pass Check: ");
            Serial.println(msg.text);

            // Fixed: Convert String to C-string for comparison
            if (strcmp(msg.text, CORRECT_PASSWORD.c_str()) == 0) {
                Serial.println("Correct! Unlocking...");
                passwd_correct();  // Unlocks and starts timer
            } else {
                Serial.println("Wrong Password");
                passwd_incorrect();               // Show error
                vTaskDelay(pdMS_TO_TICKS(3000));  // Brief error display
                lcd_locked();                     // Return to lock screen
            }
        }
    }
}

//================ INPUT =====================//


void setup() 
{
    Serial.begin(115200);

    //================ LCD STUFF ======================//
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0,0);
    //================ LCD STUFF ======================//


    //================= 2 ESPS ======================//
    Wire.begin();
    //================= 2 ESPS ======================//

    //TIMER
    doorTimer = timerBegin(1000000);
    timerAttachInterrupt(doorTimer, &onDoorTimerExpire);
    timerAlarm(doorTimer, UNLOCK_TIME_US, false, 0);
    timerStop(doorTimer);
    //TIMER


    //create queue
    xSystemQueue = xQueueCreate(10, sizeof(SystemMessage));
    if(xSystemQueue == NULL){
        Serial.println("Error creating the queue");
    }

    //================ BLE STUFF ======================//
    BLEDevice::init("MelihElliotESP");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);
    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );

    pCharacteristic->setCallbacks(new MyCallbacks());
    pService->start();
    
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();

    //================ BLE STUFF ======================//

    //================ FREERTOS TASKS ===============//
    xTaskCreatePinnedToCore(
        taskSerialInput,
        "SerialTask",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    xTaskCreatePinnedToCore(
        taskSystemManager,
        "ManagerTask",
        4096,
        NULL,
        2,
        NULL,
        1
    );


    //================ FREERTOS TASKS ===============//

}


void loop()
{

}



