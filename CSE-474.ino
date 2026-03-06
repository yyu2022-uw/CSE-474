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
#include <DHT.h>

// ================ Macros ================
#define SDA_PIN 8
#define SCL_PIN 9
#define WINDOW_BUTTON_PIN  2
#define FAN_BUTTON_PIN  3
#define SENSOR_TIMER_INTERVAL 1000000
#define MESSAGE_TIMER_INTERVAL 1000000
#define DEBOUNCE_DELAY 200

//sensors
#define DHT11_PIN 4
#define DHTTYPE DHT11
#define WATER_POWER_PIN 36
#define WATER_SIGNAL_PIN 17
#define SOUND_PIN 18
#define MOTION_PIN 27
#define HEAT_THRESHOLD 85.0


// Generate random Service and Characteristic UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID        "2405162b-b220-47e6-a767-cc5d9437ccea"
#define CHARACTERISTIC_UUID "6637ffbf-19f6-48f1-9609-888aa2951ceb"


typedef struct {
    float temp;
    float humidity;
    int water;
    int sound;
    int motion;
} SensorData;



// ============= Global Variables ============
LiquidCrystal_I2C lcd(0x27, 16, 2); 
esp_timer_handle_t sensor_timer;
esp_timer_handle_t message_timer;
volatile unsigned long lastWindowInterruptTime = 0;
volatile unsigned long lastFanInterruptTime = 0;

DHT dht(DHT11_PIN, DHTTYPE);
TaskHandle_t sensorTaskHandle = nullptr;
QueueHandle_t queue = nullptr;
bool buzzer = false;


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


void IRAM_ATTR handleWindowButtonInterrupt() { 
  if (millis() - lastWindowInterruptTime >= DEBOUNCE_DELAY) {

  }
}

void IRAM_ATTR handleFanButtonInterrupt() { 
  if (millis() - lastFanInterruptTime >= DEBOUNCE_DELAY) {

  }
}


void IRAM_ATTR sensorTimerInterrupt(void* arg) {
  vTaskNotifyGive(sensorTaskHandle);
}

void IRAM_ATTR messageTimerInterrupt(void* arg) {
  // Call xTaskNotify() to notify sensor value reading
}


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
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();

  // Sensors
  pinMode(WATER_POWER_PIN, OUTPUT);
  pinMode(WATER_SIGNAL_PIN, INPUT);
  pinMode(SOUND_PIN, INPUT);
  pinMode(MOTION_PIN, INPUT);
  dht.begin();

  // Create Sensor Tasks
  xTaskCreatePinnedToCore(sensorTask, "TaskSensor", 4096, NULL, 1, &sensorTaskHandle, 0);

  // Queue
  queue = xQueueCreate(3, sizeof(SensorData));


  // SENSOR TIMER
  esp_timer_create_args_t sensor_timer_args = {
    .callback = &sensorTimerInterrupt,
    .name = "sensor_timer"
  };
  esp_timer_create(&sensor_timer_args, &sensor_timer);
  esp_timer_start_periodic(sensor_timer, SENSOR_TIMER_INTERVAL);

  // MESSAGE TIMER
    esp_timer_create_args_t message_timer_args = {
    .callback = &messageTimerInterrupt,
    .name = "message_timer"
  };
  esp_timer_create(&message_timer_args, &message_timer);
  esp_timer_start_periodic(message_timer, MESSAGE_TIMER_INTERVAL);

  // BUTTON1
  pinMode(WINDOW_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WINDOW_BUTTON_PIN), &handleWindowButtonInterrupt, FALLING);

  // BUTTON2
  pinMode(FAN_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_BUTTON_PIN), &handleFanButtonInterrupt, FALLING);
}

void loop() {
  // FreeRTOS handles everything

}


void sensorTask(void* pvParameters){
  SensorData data;
  
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(10));
    digitalWrite(WATER_POWER_PIN, HIGH);
    data.water = analogRead(WATER_SIGNAL_PIN);
    
    digitalWrite(WATER_POWER_PIN, LOW);
    data.humidity = dht.readHumidity();
    data.temp= dht.readTemperature(true);
    data.sound = digitalRead(SOUND_PIN);
    data.motion = digitalRead(MOTION_PIN);
    
    if (dht.computeHeatIndex(data.temp, data.humidity, true) > HEAT_THRESHOLD) {
      buzzer = true;
    }

    xQueueSend(queue, &data, portMAX_DELAY); 
  }
}

void buzzerTask(void* pvParameters){
  while(1){
    if (buzzer) {
      // TODO
    }
  }
}

