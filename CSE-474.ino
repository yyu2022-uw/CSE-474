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
#define MESSAGE_TIMER_INTERVAL 100000000
#define DEBOUNCE_DELAY 200

//sensors
#define DHT11_PIN 4
#define DHTTYPE DHT11
#define WATER_POWER_PIN 17
#define WATER_SIGNAL_PIN 36
#define SOUND_PIN 18
#define MOTION_PIN 27
#define HEAT_THRESHOLD 85.0


// Generate random Service and Characteristic UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID        "2405162b-b220-47e6-a767-cc5d9437ccea"
#define CHARACTERISTIC_UUID "6637ffbf-19f6-48f1-9609-888aa2951ceb"


enum SensorType {
    TEMP,
    HUMIDITY,
    WATER,
    SOUND,
    MOTION
};

typedef struct {
    float temp;
    float humidity;
    int water;
    int sound;
    int motion;
} SensorData;

typedef struct {
    SensorType type;
    float value;
} LCDDisplay;


// ============= Global Variables ============
LiquidCrystal_I2C lcd(0x27, 16, 2); 
esp_timer_handle_t sensor_timer;
esp_timer_handle_t message_timer;
volatile unsigned long lastWindowInterruptTime = 0;
volatile unsigned long lastFanInterruptTime = 0;

DHT dht(DHT11_PIN, DHTTYPE);
TaskHandle_t sensorTaskHandle = nullptr;
TaskHandle_t messageTaskHandle = nullptr;
TaskHandle_t buzzerTaskHandle = nullptr;
QueueHandle_t lcd_queue = nullptr;

SensorData sensor_values;
SemaphoreHandle_t xSemaphore;

// False is auto mode
bool fan_mode = false;
bool fan = false;
bool window_mode = false; 
bool window = false;



// ================ Functions ================
void sensorTask(void* pvParameters);
void messageTask(void* pvParameters);
void buzzerTask(void* pvParameters);
void fanTask(void* pvParameters);
void windowTask(void* pvParameters);
void lcdTask(void* pvParameters);

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
    if (!window_mode) {
      window_mode = true;
      window = !window;
    } else {
      window_mode = false;
    }
    lastWindowInterruptTime = millis();
  }
}

void IRAM_ATTR handleFanButtonInterrupt() { 
  if (millis() - lastFanInterruptTime >= DEBOUNCE_DELAY) {
  
  if (!fan_mode) {
    fan_mode = true;
    fan = !fan;
  } else {
    fan_mode = false;
  }
    lastFanInterruptTime = millis();
  }
}


void IRAM_ATTR sensorTimerInterrupt(void* arg) {
  vTaskNotifyGiveFromISR(sensorTaskHandle, NULL);
}

void IRAM_ATTR messageTimerInterrupt(void* arg) {
  vTaskNotifyGiveFromISR(messageTaskHandle, NULL);
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
  lcd.backlight();
  lcd.clear();
  
  // Semaphore
  xSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(xSemaphore); 

  // Sensors
  pinMode(WATER_POWER_PIN, OUTPUT);
  pinMode(WATER_SIGNAL_PIN, INPUT);
  pinMode(SOUND_PIN, INPUT);
  pinMode(MOTION_PIN, INPUT);
  dht.begin();

  // Core 0
  xTaskCreatePinnedToCore(sensorTask, "TaskSensor", 4096, NULL, 1, &sensorTaskHandle, 0);
  xTaskCreatePinnedToCore(buzzerTask, "TaskBuzzer", 4096, NULL, 1, &buzzerTaskHandle, 0);
  xTaskCreatePinnedToCore(lcdTask, "TaskLCD", 4096, NULL, 1, NULL, 0);
  
  // Core 1
  xTaskCreatePinnedToCore(messageTask, "TaskMessage", 4096, NULL, 1, &messageTaskHandle, 1);
  xTaskCreatePinnedToCore(windowTask, "TaskWindow", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(fanTask, "TaskFan", 4096, NULL, 1, NULL, 1);


  // Queue
  lcd_queue = xQueueCreate(10, sizeof(LCDDisplay));


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
  SensorData local;
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    digitalWrite(WATER_POWER_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(10));
    local.water = analogRead(WATER_SIGNAL_PIN);
    digitalWrite(WATER_POWER_PIN, LOW);

    local.humidity = dht.readHumidity();
    local.temp= dht.readTemperature(true);
    local.sound = analogRead(SOUND_PIN);
    local.motion = digitalRead(MOTION_PIN); // should just be 0 or 1

    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      sensor_values = local;
      xSemaphoreGive(xSemaphore);
    }
    
    if (dht.computeHeatIndex(local.temp, local.humidity, true) > HEAT_THRESHOLD) {
      vTaskNotifyGive(buzzerTaskHandle);
    }

   
    LCDDisplay data;
    data.type = TEMP;
    data.value = local.temp;
    xQueueSend(lcd_queue, &data, portMAX_DELAY); 

    data.type = HUMIDITY;
    data.value = local.humidity;
    xQueueSend(lcd_queue, &data, portMAX_DELAY); 

    data.type = WATER;
    data.value = local.water;
    xQueueSend(lcd_queue, &data, portMAX_DELAY); 

    data.type = SOUND;
    data.value = local.sound;
    xQueueSend(lcd_queue, &data, portMAX_DELAY); 

    data.type = MOTION;
    data.value = local.motion;
    xQueueSend(lcd_queue, &data, portMAX_DELAY); 

    
    
  }
}

void messageTask(void* pvParameters){
  while(1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

  }

}


void windowTask(void* pvParameters){
  while(1) {

  }
  vTaskDelay(pdMS_TO_TICKS(100));
}


void fanTask(void* pvParameters){
  while(1) {

  }
  vTaskDelay(pdMS_TO_TICKS(100));
}

void lcdTask(void* pvParameters){
  while(1) {
    LCDDisplay receivedValue;
    if (xQueueReceive(lcd_queue, &receivedValue, portMAX_DELAY) == pdTRUE){
    
    }
  }

}

SensorData find_avg(SensorData data) {

}


void buzzerTask(void* pvParameters){
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // TODO
  }
}

