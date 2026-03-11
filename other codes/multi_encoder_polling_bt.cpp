// === 6 Motor + Encoder System (Polling, 1x decoding) ===
// Each motor has: encoder A/B, PWM pin, DIR pin
// THIS IS FOR MOTOR SHAFT ANGLE
#include "BluetoothSerial.h"
#include <ArduinoJson.h>


#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif


BluetoothSerial SerialBT;
String mac_str;

#define ppr 9048.0
#define max_angle 360.0

// Encoder pins (replace with your wiring)
const int encoderA[3] = {36, 26, 21};
const int encoderB[3] = {39, 25, 19};

// Motor control pins (replace with your wiring)
const int pwmPin[3] = {27, 27, 27};
const int dirPin[3] = {14, 14, 14};

long pulseCount[3] = {0};   // signed counters
int lastA[3] = {0};         // previous A states
unsigned long lastPrint = 0;

const int duty = 127;       // PWM duty (0–255)
float angle[3]={0,0,0};

// ================= FUNCTIONS =================

// Update encoder counts for motor i
void updateEncoder(int i) {
  int stateA = digitalRead(encoderA[i]);

  if (lastA[i] == LOW && stateA == HIGH) {
    int stateB = digitalRead(encoderB[i]);

    if (stateB == LOW) {
      pulseCount[i]++;   // CW
    } else {
      pulseCount[i]--;   // CCW
    }
  }
   if (pulseCount[i]>=ppr){
    pulseCount[i]-=ppr;
  }
  else if(pulseCount[i]<0){
    pulseCount[i]+=ppr;
  } 
  angle[i]=((float)pulseCount[i]/(ppr))*360;

  lastA[i] = stateA; 
}

// Set motor i: 'f' = forward, 'b' = backward, 's' = stop
void setMotor(int i, char cmd) {
  if (cmd == 'f') {
    digitalWrite(dirPin[i], HIGH);
    analogWrite(pwmPin[i], duty);
    Serial.print("Motor "); Serial.print(i); Serial.println(" forward");
  }
  else if (cmd == 'b') {
    digitalWrite(dirPin[i], LOW);
    analogWrite(pwmPin[i], duty);
    Serial.print("Motor "); Serial.print(i); Serial.println(" backward");
  }
  else if (cmd == 's') {
    analogWrite(pwmPin[i], 0);
    Serial.print("Motor "); Serial.print(i); Serial.println(" stopped");
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_BT");
  Serial.println("bluetooth device is ready to pair || bt name: ESP32_BT");

  mac_str = SerialBT.getBtAddressString();  // Copy the string
  Serial.print("The mac address: ");
  Serial.println(mac_str.c_str());

  // Init encoders
  for (int i = 0; i < 3; i++) {
    pinMode(encoderA[i], INPUT);
    pinMode(encoderB[i], INPUT);
    lastA[i] = digitalRead(encoderA[i]);
  }

  // Init motors
  for (int i = 0; i < 3; i++) {
    pinMode(pwmPin[i], OUTPUT);
    pinMode(dirPin[i], OUTPUT);
    analogWrite(pwmPin[i], 0);
    digitalWrite(dirPin[i], LOW);
  }

  Serial.println("3 Encoders + Motors ready!");
  Serial.println("Commands: send motorIndex (0-3) then f/b/s (e.g. '2 f')");
}

// ================= LOOP =================
void loop() {
  // Update encoders
  for (int i = 0; i < 3; i++) {
    updateEncoder(i);
  }

  // Print counts every 200 ms
  if (millis() - lastPrint >= 20) {
    //Serial.print("Counts: ");
    for (int i = 0; i < 3; i++) {
      //Serial.print("pulsecount["); 
      //Serial.print(i); 
      //Serial.print("]: "); 
     // Serial.print(pulseCount[i]);
      //Serial.print(" | angle["); 
      //Serial.print(i); 
      //Serial.print("]: "); 
      
      }
    StaticJsonDocument<128> doc;
    // Make a single array
    JsonArray arr = doc.to<JsonArray>();
    // Add all 6 angles
    for (int i = 0; i < 3; i++) {
        arr.add(angle[i]); //bc gear ratio
    }

    // Convert JSON to string
    String out;
    serializeJson(doc, out);

    // Send JSON string over Bluetooth
    SerialBT.println(out);
     
    // Also print to Serial for debugging
    Serial.println(out);

    lastPrint = millis();
}

  // Handle serial commands
  if (Serial.available() >= 2) { //checking if theres atleast 2 characters
    int motorIndex = Serial.read() - '0';  // convert '0'..'5' → int, serial.read() reads 2 if youve typed 2f, ascii val of 2 is 50, or 0 is 48 so 50-48=2
    char cmd = Serial.read();              // f/b/s
    if (motorIndex >= 0 && motorIndex < 3) {
      setMotor(motorIndex, cmd);
    }
  }
}