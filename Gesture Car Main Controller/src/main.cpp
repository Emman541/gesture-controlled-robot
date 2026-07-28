#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// Motor dirrection pins
#define IN1 16
#define IN2 17
#define IN3 18
#define IN4 19

// Motor speed pins
#define ENA 22
#define ENB 23

// Ultrasonic sensor pins
#define TRIG_LEFT 25
#define ECHO_LEFT 26
#define TRIG_RIGHT 32
#define ECHO_RIGHT 33
#define TRIG_REAR 27
#define ECHO_REAR 14

// --- SAFETY VARIABLES ---
const int SAFE_DISTANCE = 25; //
int leftDist = 100, rightDist = 100, rearDist = 100;

//Acceleration ramp value
int currentMotorSpeed = 0;   // What the motors are actually doing right now
const int RAMP_STEP = 15;    // Speed change per loop(lower = smoother)

// ESP-NOW variables
typedef struct struct_message {
  char direction;
  int speed; 
} struct_message;

struct_message receivedData;

char currentCommand = 'S'; 
int currentSpeed = 0;
unsigned long lastPacketTime = 0;
const int SIGNAL_TIMEOUT = 500; 

// Motor control functions
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void brakeMotors() {
  // Setting all IN pins HIGH while Enabled creates the electrical short (Back EMF)
  digitalWrite(IN1, HIGH); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, HIGH);
}

void driveForward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void driveBackward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void turnLeft() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); 
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  
}

void turnRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); 
}

int getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 25000); 
  if (duration == 0) return 999; 
  return duration * 0.034 / 2;
}

// --- ESP-NOW CALLBACK ---
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  currentCommand = receivedData.direction;
  currentSpeed = receivedData.speed;
  lastPacketTime = millis(); 
}

void setup() {
  Serial.begin(115200);
  
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  
  stopMotors();
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  pinMode(TRIG_LEFT, OUTPUT); pinMode(ECHO_LEFT, INPUT);
  pinMode(TRIG_RIGHT, OUTPUT); pinMode(ECHO_RIGHT, INPUT);
  pinMode(TRIG_REAR, OUTPUT); pinMode(ECHO_REAR, INPUT);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Read Sensors
  leftDist = getDistance(TRIG_LEFT, ECHO_LEFT);
  delay(10); 
  rightDist = getDistance(TRIG_RIGHT, ECHO_RIGHT);
  delay(10);
  rearDist = getDistance(TRIG_REAR, ECHO_REAR);
  delay(10);

  bool frontBlocked = (leftDist > 0 && leftDist <= SAFE_DISTANCE) || 
                      (rightDist > 0 && rightDist <= SAFE_DISTANCE);
  bool rearBlocked = (rearDist > 0 && rearDist <= SAFE_DISTANCE);

  //Fetch Latest Command from glove
  char activeCommand = currentCommand;
  int targetSpeed = currentSpeed; 

  // Safety Overrides & Turning Logic
  if (millis() - lastPacketTime > SIGNAL_TIMEOUT) {
    activeCommand = 'S'; 
    targetSpeed = 0;
  } else if (activeCommand == 'F' && frontBlocked) {
    activeCommand = 'X'; // Trigger active braking
    targetSpeed = 255;   // Maximum power required to lock the motors
  } else if (activeCommand == 'B' && rearBlocked) {
    activeCommand = 'X';
    targetSpeed = 255;
  } else if (activeCommand == 'L' || activeCommand == 'R') {
    // Cap the max speed during turns to prevent violent spinning
    if (targetSpeed > 150) {
      targetSpeed = 150; 
    }
  }

  //  Acceleration Ramp 
  if (activeCommand == 'S' || activeCommand == 'X') {
    // Bypass the ramp for immediate braking or coasting
    currentMotorSpeed = targetSpeed; 
  } else {
    // Smoothly increase or decrease speed towards the target
    if (currentMotorSpeed < targetSpeed) {
      currentMotorSpeed += RAMP_STEP;
      if (currentMotorSpeed > targetSpeed) currentMotorSpeed = targetSpeed;
    } else if (currentMotorSpeed > targetSpeed) {
      currentMotorSpeed -= RAMP_STEP;
      if (currentMotorSpeed < targetSpeed) currentMotorSpeed = targetSpeed;
    }
  }

  // Execute Hardware Signals
  analogWrite(ENA, currentMotorSpeed);
  analogWrite(ENB, currentMotorSpeed);

  switch (activeCommand) {
    case 'F': driveForward(); break;
    case 'B': driveBackward(); break;
    case 'L': turnLeft(); break;
    case 'R': turnRight(); break;
    case 'S': stopMotors(); break;  // Coasting
    case 'X': brakeMotors(); break; // Active Braking
  }
}