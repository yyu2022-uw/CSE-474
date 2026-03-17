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
#include <ESP32Servo.h>
#include <Stepper.h>

// ================ Macros ================
#define SDA_PIN 8
#define SCL_PIN 9
#define WINDOW_BUTTON_PIN  40
#define FAN_BUTTON_PIN  41
#define SENSOR_TIMER_INTERVAL 3000000
#define MESSAGE_TIMER_INTERVAL 100000000
#define DEBOUNCE_DELAY 200

// Sensors
#define DHT11_PIN 4
#define DHTTYPE DHT11
#define WATER_SIGNAL_PIN 1
#define SOUND_PIN 18
#define MOTION_PIN 5
#define HEAT_THRESHOLD 85
#define TEMP_LIMIT 80
#define HUMIDITY_LIMIT 70

#define WATER_LOW 500
#define WATER_HIGH 1000
#define SOUND_LOW 200
#define SOUND_HIGH 300
// Buzzer
#define BUZZER_PIN 15
#define LEDC_RES 8
#define LEDC_FREQ 2000

// Stepper motor
#define IN1 10
#define IN2 11
#define IN3 12
#define IN4 13
#define STEPS_PER_REV 512

// Servo motor
#define SERVO_PIN 16
#define WINDOW_OPEN 90
#define WINDOW_CLOSE 0

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
  float water;
  float sound;
  float motion;
} SensorData;

typedef struct {
  SensorType type;
  float value;
} LCDValue;


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
TaskHandle_t fanTaskHandle = nullptr;
QueueHandle_t lcd_queue = nullptr;


SemaphoreHandle_t xSemaphore = nullptr;

SensorData sensor_values;
SensorData sensor_avg;
int sample_size = 0;
BLECharacteristic *pCharacteristic = nullptr;

volatile bool fan_mode = false; // False is auto mode
volatile bool fan = false;
volatile bool window_button_pressed = false;
volatile bool window_mode = false; 
volatile bool window = false;

Stepper fan_stepper(STEPS_PER_REV, IN1, IN2, IN3, IN4);
Servo window_servo; 

// ================ Prototypes ================
void sensorTask(void* pvParameters);
void messageTask(void* pvParameters);
void buzzerTask(void* pvParameters);
void fanTask(void* pvParameters);
void windowTask(void* pvParameters);
void lcdTask(void* pvParameters);
void findMovingAverage(SensorData data);


// ================ Functions ================
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
    vTaskNotifyGiveFromISR(fanTaskHandle, NULL);
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
  // BLE
  BLEDevice::init("MyESP32");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                                          CHARACTERISTIC_UUID,
                                          BLECharacteristic::PROPERTY_READ |
                                          BLECharacteristic::PROPERTY_NOTIFY
                                        );
 
  pCharacteristic->setValue("Starting...");
  pService->start();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();

  // LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Mutex
  xSemaphore = xSemaphoreCreateMutex();

  // Sensors
  pinMode(WATER_SIGNAL_PIN, INPUT);
  pinMode(SOUND_PIN, INPUT);
  pinMode(MOTION_PIN, INPUT);
  dht.begin();

  sensor_values.humidity = 0;
  sensor_values.motion = 0;
  sensor_values.sound = 0;
  sensor_values.temp = 0;
  sensor_values.water = 0;

  sensor_avg.humidity = 0;
  sensor_avg.motion = 0;
  sensor_avg.sound = 0;
  sensor_avg.temp = 0;
  sensor_avg.water = 0;

  // Stepper motor
  fan_stepper.setSpeed(30);

  // Server motor
  window_servo.attach(SERVO_PIN);
  window_servo.write(WINDOW_CLOSE);

  // Buzzer
  ledcAttach(BUZZER_PIN, LEDC_FREQ, LEDC_RES);
  ledcWrite(BUZZER_PIN, 0);

  // Queue
  lcd_queue = xQueueCreate(10, sizeof(LCDValue));

  // Window button
  pinMode(WINDOW_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WINDOW_BUTTON_PIN), &handleWindowButtonInterrupt, FALLING);

  // Fan button
  pinMode(FAN_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_BUTTON_PIN), &handleFanButtonInterrupt, FALLING);

  // Core 0
  xTaskCreatePinnedToCore(sensorTask, "TaskSensor", 4096, NULL, 1, &sensorTaskHandle, 0);
  xTaskCreatePinnedToCore(buzzerTask, "TaskBuzzer", 4096, NULL, 1, &buzzerTaskHandle, 0);
  xTaskCreatePinnedToCore(lcdTask, "TaskLCD", 4096, NULL, 1, NULL, 0);
  
  // Core 1
  xTaskCreatePinnedToCore(messageTask, "TaskMessage", 4096, NULL, 1, &messageTaskHandle, 1);
  xTaskCreatePinnedToCore(windowTask, "TaskWindow", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(fanTask, "TaskFan", 4096, NULL, 1, &fanTaskHandle, 1);

  // Sensor timer
  esp_timer_create_args_t sensor_timer_args = {
    .callback = &sensorTimerInterrupt,
    .name = "sensor_timer"
  };
  esp_timer_create(&sensor_timer_args, &sensor_timer);
  esp_timer_start_periodic(sensor_timer, SENSOR_TIMER_INTERVAL);

  // Message timer
    esp_timer_create_args_t message_timer_args = {
    .callback = &messageTimerInterrupt,
    .name = "message_timer"
  };
  esp_timer_create(&message_timer_args, &message_timer);
  esp_timer_start_periodic(message_timer, MESSAGE_TIMER_INTERVAL);

}

void sensorTask(void* pvParameters){
  SensorData local;
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(10));
    local.water = analogRead(WATER_SIGNAL_PIN);

    local.humidity = dht.readHumidity();
    local.temp= dht.readTemperature(true);
    local.sound = analogRead(SOUND_PIN);
    local.motion = digitalRead(MOTION_PIN); // should just be 0 or 1

    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      sensor_values = local;
      findMovingAverage(local);
      xSemaphoreGive(xSemaphore);
    }
    
    if (dht.computeHeatIndex(local.temp, local.humidity, true) > HEAT_THRESHOLD) {
      xTaskNotifyGive(buzzerTaskHandle);
    }
   
    LCDValue data;
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
  SensorData avg;
  while(1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      avg = sensor_avg;
      xSemaphoreGive(xSemaphore);
    }

    String msg = "Temp: " + String(avg.temp, 1) +
                 ", Humidity: " + String(avg.humidity, 1) +
                 ", Water: " + String(avg.water, 1) +
                 ", Sound: " + String(avg.sound, 1) +
                 ", Motion: " + String(avg.motion, 1);

    pCharacteristic->setValue(msg.c_str());
    pCharacteristic->notify();
  }
}

void findMovingAverage(SensorData data) {
  if (sample_size == 0) {
    sensor_avg = data;
    sample_size = 1;
  } else {
    sensor_avg.temp = (sensor_avg.temp * sample_size + data.temp) / (sample_size + 1);
    sensor_avg.humidity = (sensor_avg.humidity * sample_size + data.humidity) / (sample_size + 1);
    sensor_avg.water = (sensor_avg.water * sample_size + data.water) / (sample_size + 1);
    if ((sensor_avg.motion * sample_size + data.motion) / (sample_size + 1) >= 0.5) {
      sensor_avg.motion = 1;
    } else {
      sensor_avg.motion = 0;
    }
    sensor_avg.sound = (sensor_avg.sound * sample_size + data.sound) / (sample_size + 1);
    sample_size++;
  }
}

void windowTask(void* pvParameters){
  float curr_humidity;
  while(1) {
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      curr_humidity = sensor_values.humidity;
      xSemaphoreGive(xSemaphore);
    }

    if (window_mode) {
      // manual mode
      if (window) {
        window_servo.write(WINDOW_OPEN);
      } else {
        window_servo.write(WINDOW_CLOSE);
      }
    } else {
      // auto mode
      if (curr_humidity >= HUMIDITY_LIMIT) {
        window = true;
        window_servo.write(WINDOW_OPEN);
      } else {
        window = false;
        window_servo.write(WINDOW_CLOSE);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void fanTask(void* pvParameters){
  float curr_temp;

  while(1) {
    if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
      if (!fan_mode) {
        fan_mode = true;   
        fan = !fan;        
      } else {
        fan_mode = false;  
      }
    }

    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      curr_temp = sensor_values.temp;
      xSemaphoreGive(xSemaphore);
    }

    if (fan_mode) {
      // manual mode
      if (fan) {
        fan_stepper.step(8);
      }
    } else {
      // auto mode
      if (curr_temp >= TEMP_LIMIT) {
        fan_stepper.step(8);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void lcdTask(void* pvParameters){
  while(1) {
    LCDValue receivedValue;
    if (xQueueReceive(lcd_queue, &receivedValue, portMAX_DELAY) == pdTRUE){
      lcd.clear();
      lcd.setCursor(0, 0);

      switch (receivedValue.type) {
        case TEMP:
          lcd.print("Temperature: "); 
          lcd.setCursor(0, 1);
          lcd.print(receivedValue.value);      
          lcd.print(" F");     
          break;
        
        case HUMIDITY:
          lcd.print("Humidity: "); 
          lcd.setCursor(0, 1);
          lcd.print(receivedValue.value);          
          lcd.print(" %"); 
          break;
        
        case WATER:
          lcd.print("Water: "); 
          if (receivedValue.value < WATER_LOW){
            lcd.print("Low");
          } else if(receivedValue.value > WATER_HIGH){
            lcd.print("High");
          } else {
            lcd.print("Medium");
          }         
          break;
        
        case SOUND:
          lcd.print("Sound: "); 
          if (receivedValue.value < SOUND_LOW){
            lcd.print("Low");
          } else if(receivedValue.value > SOUND_HIGH) {
            lcd.print("High");  
          } else {
            lcd.print("medium");   
          }    
          break;
        
        case MOTION:
          lcd.print("Motion: "); 
          lcd.setCursor(0, 1);
          if (receivedValue.value >= 0.5){
            lcd.print("Detected");
          } else{
            lcd.print("None");    
          }      
          break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void buzzerTask(void* pvParameters){
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ledcWrite(BUZZER_PIN, 128);
    vTaskDelay(pdMS_TO_TICKS(1000));
    ledcWrite(BUZZER_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void loop() {
  // FreeRTOS handles everything
}
