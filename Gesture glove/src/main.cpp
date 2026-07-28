#include <Arduino.h>
#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>

// PIN DEF
#define I2C_SDA 10
#define I2C_SCL 9
#define RGB_LED_PIN 8

// Tilt sensitivities
// Lower number = more sensitive. Higher number = Less sensitive
int TILT_FORWARD  = 6000; 
int TILT_BACKWARD = 8000;
int TILT_LEFT     = 8000;
int TILT_RIGHT    = 8000;

// The absolute maximum tilt before it hit 100% top speed
int MAX_TILT = 16000; 

// The minimum PWM required to make the motors physically spin (0-255)
int MIN_SPEED = 100; 

// MAC address of Receiver ESP (ESP32-U in the Car)
uint8_t carAddress[] = {0xB4, 0xBF, 0xE9, 0x10, 0xFA, 0xA8};

// Data packet
typedef struct struct_message {
  char direction; 
  int speed; //Holds the calculated 0-255 speed value
} struct_message;

struct_message commandData;
esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    neopixelWrite(RGB_LED_PIN, 0, 40, 0); // Green if connected
  } else {
    neopixelWrite(RGB_LED_PIN, 40, 0, 0); // Red if disconnected
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000); 
  
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.beginTransmission(0x68);
  Wire.write(0x6B); 
  Wire.write(0);    
  Wire.endTransmission();
  
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb(OnDataSent);
  
  memcpy(peerInfo.peer_addr, carAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  esp_now_add_peer(&peerInfo);
  neopixelWrite(RGB_LED_PIN, 0, 0, 40); // Boot-up Blue
}

void loop() {
  Wire.beginTransmission(0x68);
  Wire.write(0x3B); 
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 14, true); 

  if (Wire.available() == 14) {
    int16_t AcX = Wire.read() << 8 | Wire.read();
    int16_t AcY = Wire.read() << 8 | Wire.read();
    
    // Ignore Z-axis and Gyro for basic tilt logic
    for(int i=0; i<10; i++) Wire.read(); 

    commandData.direction = 'S'; 
    commandData.speed = 0;

    // Apply Orientation Logic and Map Speed dynamically
    if (AcY < -TILT_FORWARD) {
      commandData.direction = 'F';        
      commandData.speed = map(-AcY, TILT_FORWARD, MAX_TILT, MIN_SPEED, 255);
    } 
    else if (AcY > TILT_BACKWARD) {
      commandData.direction = 'B';        
      commandData.speed = map(AcY, TILT_BACKWARD, MAX_TILT, MIN_SPEED, 255);
    } 
    else if (AcX > TILT_RIGHT) {
      commandData.direction = 'L';  
      commandData.speed = map(AcX, TILT_RIGHT, MAX_TILT, MIN_SPEED, 255);
    } 
    else if (AcX < -TILT_LEFT) {
      commandData.direction = 'R';   
      commandData.speed = map(-AcX, TILT_LEFT, MAX_TILT, MIN_SPEED, 255);
    }

    //Prevent math overflow from crashing the PWM signal
    commandData.speed = constrain(commandData.speed, 0, 255);

    Serial.print("Cmd: "); Serial.print(commandData.direction); 
    Serial.print(" | Speed: "); Serial.println(commandData.speed);

    esp_now_send(carAddress, (uint8_t *) &commandData, sizeof(commandData));
  }
  delay(100); // Slightly faster polling for smoother throttle response
}