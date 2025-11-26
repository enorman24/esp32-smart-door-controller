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



//================ Password ==================//
// const char


//================ BLE STUFF ======================//
#define SERVICE_UUID        "56be7035-a164-4929-854d-ab699176f9fd"
#define CHARACTERISTIC_UUID "8e4b2914-ba59-432f-a51b-b66dee7838b7"

class MyCallbacks: public BLECharacteristicCallbacks {
   void onWrite(BLECharacteristic *pCharacteristic) override {
     (void)pCharacteristic;  // Characteristic not used past notification
     messageFlag = true;
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


//================ SERIAL INPUT =====================//
void read_password() {
    static String curr = "";  // Buffer accumulating the current line
    if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            if(curr == PASSWORD) {
                passwd_correct();
            } else {
                passwd_incorrect();
            }
            curr = "";
        } else if (c != '\r') {  // ignore carriage returns from Windows line endings
            curr += c;
        }
    }
}
//================ SERIAL INPUT =====================//


//================ PASSWORD =====================//
//Display on LCD, Disarm alarm, unlock door
void passwd_correct(){

}

//Display on LCD, set off alarm (set amount of time)
void passwd_incorrect(){

}
//================ PASSWORD =====================//





setup() 
{
    Serial.begin(115200);

    //================ LCD STUFF ======================//
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0,0);
    //================ LCD STUFF ======================//


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
    // xTaskCreatePinnedToCore(
    //     taskLightDetector, 
    //     "User Input Task",
    //     4096,
    //     NULL,
    //     1,
    //     NULL,
    //     0
    // );

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


loop()
{

}