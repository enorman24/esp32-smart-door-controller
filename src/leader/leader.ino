/*
PURPOSE
- Control the display
- Watch for user input (BLE/Serial)
- Tell ESP32 #2 to open/close door
- Listen for inputs from ESP32 for sensors
*/

#include <Arduino.h>
#include <LiquidCrystal_I2C.h> //for using the lcd display
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" //for tasks
#include <Wire.h> // for using 2 esp32s
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h> 


#define FOLLOWER_ADDR 8
#define I2C_SDA_PIN 8 
#define I2C_SCL_PIN 9
//================ Password ==================//
const String PASSWORD = "12345"; 

//================ BLE STUFF ======================//
#define SERVICE_UUID        "56be7035-a164-4929-854d-ab699176f9fd"
#define CHARACTERISTIC_UUID "8e4b2914-ba59-432f-a51b-b66dee7838b7"

volatile bool bleInputReceived = false;
String bleInput = "";

class MyCallbacks: public BLECharacteristicCallbacks {
   void onWrite(BLECharacteristic *pCharacteristic) override {
     std::string rxValue = pCharacteristic->getValue();
     
     if (rxValue.length() > 0) {
       bleInput = String(rxValue.c_str());
       bleInput.trim();
       bleInputReceived = true;
     }
   }
};
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
    lcd.print("Wrong Passwd");
}
//================ LCD STUFF ======================//

//================ PASSWORD =====================//

void passwd_correct(){
    lcd_correct(); //display on lcd
    Wire.beginTransmission(FOLLOWER_ADDR); //send message to esp32 #2
    Wire.write(1);
    Wire.endTransmission();
}

void passwd_incorrect(){
    lcd_incorrect();
}
//================ PASSWORD =====================//

//================== INPUT =====================//
void check_serial() {
    static String curr = "";  
    if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            curr.trim(); 
            if(curr == PASSWORD) {
                passwd_correct();
            } else {
                passwd_incorrect();
            }
            curr = "";
        } else if (c != '\r') {  
            curr += c;
        }
    }
}

void taskInputHandler(void * parameter) {
        while(1) {
            check_serial();
            if (bleInputReceived) {                
                if (bleInput == PASSWORD) {
                    passwd_correct();
                } else {
                    passwd_incorrect();
                }
                bleInputReceived = false; 
                bleInput = "";            
            }
            vTaskDelay(50 / portTICK_PERIOD_MS);
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

    //================ I2C ======================//
    setPins(int sI2C_SDA_PIN, int I2C_SCL_PIN);
     Wire.begin();
    //================ I2C ======================//


    //================= 2 ESPS ======================//
    Wire.begin();
    //================= 2 ESPS ======================//


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
        taskInputHandler,
        "Input Task",
        4096,
        NULL,  
        1,
        NULL,               
        1                   
    );

    //  xTaskCreatePinnedToCore(
    //     taskLCD,
    //     "LCD Display Task",
    //     4096,
    //     NULL,
    //     1,
    //     NULL,
    //     0
    // );
    //================ FREERTOS TASKS ===============//

}


void loop()
{

}