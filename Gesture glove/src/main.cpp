#include <Arduino.h>
#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>

#define I2C_SDA 8
#define I2C_SCL 9
const int MPU_ADDR = 0x68;

// Sensitivity adjustment (Lower = more sensitive, Higher = less sensitive)
int TILT_THRESHOLD = 8000;

// Receiver address
uint8_t carAddress[] = {0xB4, 0xBF, 0xE9, 0x10, 0xFA, 0xA8};

typedef struct struct_message {
  char direction; 
} struct_message;

struct_message commandData;
esp_now_peer_info_t peerInfo;

// Callback to check delivery status
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Delivery: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  delay(2000); 
  
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); 
  Wire.write(0);    
  Wire.endTransmission();
  
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  
  memcpy(peerInfo.peer_addr, carAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  
  Serial.println("Gesture Transmitter Locked onto Car!");
}

void loop() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); 
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true); 

  if (Wire.available() == 14) {
    int16_t AcX = Wire.read() << 8 | Wire.read();
    int16_t AcY = Wire.read() << 8 | Wire.read();
    int16_t AcZ = Wire.read() << 8 | Wire.read();
    int16_t Tmp = Wire.read() << 8 | Wire.read(); 
    int16_t GyX = Wire.read() << 8 | Wire.read();
    int16_t GyY = Wire.read() << 8 | Wire.read();
    int16_t GyZ = Wire.read() << 8 | Wire.read();

    commandData.direction = 'S'; // Default to STOP

    // Apply Logic(For direction)
    if (AcX > TILT_THRESHOLD) {
      commandData.direction = 'R';        
    } else if (AcX < -TILT_THRESHOLD) {
      commandData.direction = 'L';        
    } else if (AcY < -TILT_THRESHOLD) {
      commandData.direction = 'F';        
    } else if (AcY > TILT_THRESHOLD) {
      commandData.direction = 'B';        
    }

    Serial.print("Gesture: "); Serial.print(commandData.direction); Serial.print("  |  ");

    // Send directions to the receiving ESP
    esp_now_send(carAddress, (uint8_t *) &commandData, sizeof(commandData));
  }
  
  delay(150); 
}