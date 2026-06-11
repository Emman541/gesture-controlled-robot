#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// This MUST match the structure on the transmitter glove exactly
typedef struct struct_message {
  char direction; 
} struct_message;

//variable to hold the incoming data
struct_message receivedData;

// Callback function that automatically triggers when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  // Unpack the incoming bytes into our structured variable
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  
  // Print the message received!
  Serial.print("Glove gesture ---> ");
  Serial.println(receivedData.direction);
}

void setup() {
  Serial.begin(115200);
  delay(2000); //
  
  //Set as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  //Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Tell ESP-NOW to use our OnDataRecv function when a packet arrives
  esp_now_register_recv_cb(OnDataRecv);
  
  Serial.println("Waiting for gesture commands...");
}

void loop() {
  /* The OnDataRecv function handles everything in the background the exact instant a message arrives. */
  
  delay(1000); 
}