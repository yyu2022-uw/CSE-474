// ================ Includes ================
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h" 
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "esp_timer.h"

// ================ Macros ================
#define SDA_PIN 8
#define SCL_PIN 9
#define BUTTON1_PIN  2
#define BUTTON2_PIN  3

// Generate random Service and Characteristic UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID        "2405162b-b220-47e6-a767-cc5d9437ccea"
#define CHARACTERISTIC_UUID "6637ffbf-19f6-48f1-9609-888aa2951ceb"

// ============= Global Variables ============
LiquidCrystal_I2C lcd(0x27, 16, 2); 
esp_timer_handle_t periodic_timer;
const unsigned long debounceDelay = 200; 


// ================ Functions ================
class MyCallbacks: public BLECharacteristicCallbacks {
  // Name: onWrite
  // Description: sets flags to display a BLE message on the LCD and pause the timer updates
  void onWrite(BLECharacteristic *pCharacteristic) {
    // =========> TODO: This callback function will be invoked when signal is
    // 		     received over BLE. Implement the necessary functionality that
    //		     will trigger the message to the LCD.
  }
};


void setup() {
  // Serial
  Serial.begin(115200);

  // BLE
  BLEDevice::init("MyESP32");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                          CHARACTERISTIC_UUID,
                                          BLECharacteristic::PROPERTY_READ |
                                          BLECharacteristic::PROPERTY_WRITE
                                        );


  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();

  // LCD
  Wire.begin();
  lcd.init();

  // TIMER
  esp_timer_create_args_t timer_args = {
    .callback = &timerInterrupt,
    .name = "my_timer"
  };
  esp_timer_create(&timer_args, &periodic_timer);
  esp_timer_start_periodic(periodic_timer, TIMER_INTERVAL);

  // BUTTON1
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON1_PIN), &handleButtonInterrupt, FALLING);

}

void loop() {
  // FreeRTOS handles everything

}
