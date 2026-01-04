#include <esp_now.h>
#include <WiFi.h>
#include <esp_bt.h>

// REPLACE WITH YOUR RECEIVER'S MAC Address
// To find MAC address, upload a sketch with: Serial.println(WiFi.macAddress());
uint8_t receiverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Structure to send motor control data
// Must match the receiver structure EXACTLY
typedef struct struct_message {
  int motorCommand;  // 0=STOP, 1=FORWARD, 2=REVERSE
  bool cruiseActive; // Is cruise control active?
  int speed;         // Speed value (0-255) from joystick
  int joystickX;     // Raw joystick X-axis value (0-4095)
} struct_message;

// Create a struct_message called controlData
struct_message controlData;

esp_now_peer_info_t peerInfo;

// --- PINS ---
const int PIN_FWD_BTN = 2; 
const int PIN_REV_BTN = 4; 
const int PIN_SW_BTN  = 5; // Acts as "Cruise Control" Toggle
const int PIN_JOYSTICK_X = 34; // Analog pin for joystick X-axis (ADC1)
const int PIN_JOYSTICK_Y = 35; // Analog pin for joystick Y-axis for speed (ADC1)

// --- JOYSTICK CONFIGURATION ---
const int JOYSTICK_CENTER = 2048; // Center position for 12-bit ADC (0-4095)
const int JOYSTICK_DEADZONE = 200; // Deadzone around center
const int JOYSTICK_MIN = 0;
const int JOYSTICK_MAX = 4095;

// --- STATE VARIABLES ---
volatile bool rawFwdState = false;
volatile bool rawRevState = false;
volatile bool switchEvent = false;
volatile unsigned long lastSwitchPressTime = 0;
bool cruiseMode = false;
bool lastFwdState = false;

// --- DEBOUNCE ---
unsigned long lastDebounceTime = 0;
const int DEBOUNCE_DELAY = 300;

// --- MOTOR COMMAND STATES ---
const int CMD_STOP = 0;
const int CMD_FORWARD = 1;
const int CMD_REVERSE = 2;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}

// --- FUNCTION PROTOTYPES ---
void IRAM_ATTR isrForward();
void IRAM_ATTR isrReverse();
void IRAM_ATTR isrSwitch();
void sendMotorCommand(int command);
int readJoystickX();
int readJoystickY();
int mapJoystickToSpeed(int joystickValue);

void setup() {
  Serial.begin(115200);
  
  // Disable Bluetooth to save power
  btStop();
  esp_bt_controller_disable();
  Serial.println("Bluetooth disabled for power saving");
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  
  // Print MAC Address for reference
  Serial.print("Remote MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register send callback
  esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
  
  // Register peer (the receiver ESP32)
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  
  Serial.println("ESP-NOW Initialized Successfully");
  
  // Setup button pins
  pinMode(PIN_FWD_BTN, INPUT);
  pinMode(PIN_REV_BTN, INPUT);
  pinMode(PIN_SW_BTN,  INPUT);
  
  // Setup joystick pin (analog input)
  pinMode(PIN_JOYSTICK_X, INPUT);
  pinMode(PIN_JOYSTICK_Y, INPUT);
  
  // Configure ADC for 12-bit resolution
  analogReadResolution(12); // 0-4095 range
  analogSetAttenuation(ADC_11db); // Full range 0-3.3V
  
  // Attach Interrupts
  attachInterrupt(digitalPinToInterrupt(PIN_FWD_BTN), isrForward, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_REV_BTN), isrReverse, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_SW_BTN),  isrSwitch,  RISING);
  
  Serial.println("=================================");
  Serial.println("Remote Control Ready");
  Serial.println("=================================");
  Serial.println("Forward Button (PIN 2): Hold to go (Gas Pedal)");
  Serial.println("  - Pressing Forward while Cruise is ON will disable Cruise");
  Serial.println("Switch Button (PIN 5): Toggle Cruise Control");
  Serial.println("Reverse Button (PIN 4): Hold to reverse");
  Serial.println("Joystick X-axis (PIN 34): Control steering");
  Serial.println("Joystick Y-axis (PIN 35): Control speed (0-255)");
  Serial.println("=================================");
}

void loop() {
  // Read joystick axes
  int joystickX = readJoystickX();
  int joystickY = readJoystickY();
  int speed = mapJoystickToSpeed(joystickY); // Use Y-axis for speed
  
  // 1. CHECK IF FORWARD BUTTON WAS JUST PRESSED (while cruise is on)
  if (rawFwdState && !lastFwdState && cruiseMode) {
    cruiseMode = false;
    Serial.println(">> CRUISE CONTROL: DISABLED (Forward Button Pressed) <<");
  }
  lastFwdState = rawFwdState;
  
  // 2. HANDLE SWITCH TOGGLE (CRUISE CONTROL)
  if (switchEvent) {
    unsigned long currentTime = millis();
    
    if ((currentTime - lastDebounceTime > DEBOUNCE_DELAY) && 
        digitalRead(PIN_SW_BTN) == HIGH) {
      
      cruiseMode = !cruiseMode;
      
      if (cruiseMode) {
        Serial.println(">> CRUISE CONTROL: ENABLED <<");
      } else {
        Serial.println(">> CRUISE CONTROL: DISABLED <<");
        // Send stop command when cruise is disabled
        sendMotorCommand(CMD_STOP);
      }
      
      lastDebounceTime = currentTime;
    }
    switchEvent = false;
  }
  
  // 3. SAFETY CHECKS
  
  // SAFETY: Both Physical Buttons Pressed
  if (rawFwdState && rawRevState) {
    Serial.println("! SAFETY STOP ! (Both Buttons Pressed)");
    sendMotorCommand(CMD_STOP);
    return;
  }
  
  // SAFETY: Cruise Control is ON, but User Pressed Reverse
  if (cruiseMode && rawRevState) {
    Serial.println("! SAFETY STOP ! (Cruise + Reverse Conflict)");
    cruiseMode = false; 
    Serial.println(">> CRUISE CONTROL: AUTO-DISABLED (Safety) <<");
    sendMotorCommand(CMD_STOP);
    return;
  }
  
  // 4. MOTOR CONTROL LOGIC & SEND COMMANDS
  
  bool wantToGoForward = rawFwdState || cruiseMode;
  bool wantToGoReverse = rawRevState;
  
  if (wantToGoForward) {
    if (cruiseMode) {
      Serial.print("Action: DRIVING FORWARD >>> (CRUISE) | Speed: ");
      Serial.print(speed);
      Serial.print(" | Joystick: ");
      Serial.println(joystickX);
      sendMotorCommand(CMD_FORWARD);
    } else {
      Serial.print("Action: DRIVING FORWARD >>> (BUTTON HELD) | Speed: ");
      Serial.print(speed);
      Serial.print(" | Joystick: ");
      Serial.println(joystickX);
      sendMotorCommand(CMD_FORWARD);
    }
  }
  else if (wantToGoReverse) {
    Serial.print("Action: DRIVING REVERSE <<< | Speed: ");
    Serial.print(speed);
    Serial.print(" | Joystick: ");
    Serial.println(joystickX);
    sendMotorCommand(CMD_REVERSE);
  }
  else {
    sendMotorCommand(CMD_STOP);
  }
  
  delay(50); // Send updates every 50ms
}

// Function to read joystick Y-axis with averaging for stability
int readJoystickY() {
  const int samples = 5;
  long sum = 0;
  
  for (int i = 0; i < samples; i++) {
    sum += analogRead(PIN_JOYSTICK_Y);
    delayMicroseconds(100);
  }
  
  return sum / samples;
}

// Function to read joystick X-axis with averaging for stability
int readJoystickX() {
  const int samples = 5;
  long sum = 0;
  
  for (int i = 0; i < samples; i++) {
    sum += analogRead(PIN_JOYSTICK_X);
    delayMicroseconds(100);
  }
  
  return sum / samples;
}

// Function to map joystick position to speed (0-255)
int mapJoystickToSpeed(int joystickValue) {
  // Apply deadzone around center
  if (abs(joystickValue - JOYSTICK_CENTER) < JOYSTICK_DEADZONE) {
    return 128; // Mid speed when centered
  }
  
  // Map joystick range to speed (0-255)
  int speed = map(joystickValue, JOYSTICK_MIN, JOYSTICK_MAX, 0, 255);
  
  // Constrain to valid range
  speed = constrain(speed, 0, 255);
  
  return speed;
}

// Function to send motor commands via ESP-NOW
void sendMotorCommand(int command) {
  static int lastCommand = -1;
  static int lastSpeed = -1;
  static int lastJoystickX = -1;
  
  // Read current joystick positions and speed
  int currentJoystickX = readJoystickX();
  int currentJoystickY = readJoystickY();
  int currentSpeed = mapJoystickToSpeed(currentJoystickY); // Use Y for speed
  
  // Send if command, speed, or joystick position changed significantly
  bool shouldSend = (command != lastCommand) || 
                    (abs(currentSpeed - lastSpeed) > 5) ||
                    (abs(currentJoystickX - lastJoystickX) > 50);
  
  if (shouldSend) {
    controlData.motorCommand = command;
    controlData.cruiseActive = cruiseMode;
    controlData.speed = currentSpeed;
    controlData.joystickX = currentJoystickX;
    
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &controlData, sizeof(controlData));
    
    if (result != ESP_OK) {
      Serial.println("Error sending command");
    }
    
    lastCommand = command;
    lastSpeed = currentSpeed;
    lastJoystickX = currentJoystickX;
  }
}

// --- INTERRUPT SERVICE ROUTINES ---
void IRAM_ATTR isrForward() {
  rawFwdState = digitalRead(PIN_FWD_BTN);
}

void IRAM_ATTR isrReverse() {
  rawRevState = digitalRead(PIN_REV_BTN);
}

void IRAM_ATTR isrSwitch() {
  unsigned long currentTime = millis();
  if (currentTime - lastSwitchPressTime > DEBOUNCE_DELAY) {
    switchEvent = true;
    lastSwitchPressTime = currentTime;
  }
}