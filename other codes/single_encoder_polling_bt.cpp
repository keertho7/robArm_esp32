#include <Arduino.h>
// === Single Encoder Reader (Polling, 1x decoding) ===
// Counts only rising edges of channel A
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif


BluetoothSerial SerialBT;
String mac_str;

#define pwm1 27
#define dir1 14
#define ppr 9048.0
#define max_angle 360.0
const int encoderA = 34;
const int encoderB = 35;

long pulseCount = 0;           // signed (forward/backward)
int lastA = LOW;               // stores previous A state
unsigned long lastPrint = 0;   // global, persists automatically

const int duty =127;
float angle=0.0;


void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_BT");
  Serial.println("bluetooth device is ready to pair || bt name: ESP32_BT");

  mac_str = SerialBT.getBtAddressString();  // Copy the string
  Serial.print("The mac address: ");
  Serial.println(mac_str.c_str());

  pinMode(encoderA, INPUT);
  pinMode(encoderB, INPUT);
  pinMode(pwm1, OUTPUT);
  pinMode(dir1, OUTPUT);

   // Start motor off
  analogWrite(pwm1, 0);
  digitalWrite(dir1, LOW);

  Serial.println("Encoder counts (polling) -----");
  Serial.println("Enter command: f = forward, b = backward, s = stop");
}

void loop() {
  int stateA = digitalRead(encoderA);

  // Detect rising edge of A
  if (lastA == LOW && stateA == HIGH) {
    int stateB = digitalRead(encoderB);

    // Direction check
    if (stateB == LOW) {
      pulseCount++;   // CW
    } else {
      pulseCount--;   // CCW
    }
  }

  if (pulseCount>=ppr){
    pulseCount-=ppr;
  }
  else if(pulseCount<0){
    pulseCount+=ppr;
  }
  angle=((float)pulseCount/ppr)*360;

  lastA = stateA;

  // Print every 20 ms
  if (millis() - lastPrint >= 20) {
    Serial.print("pulsecount: "); Serial.print(pulseCount);
    Serial.print(" | angle: "); Serial.println(angle);
    
    SerialBT.print("pulsecount: "); SerialBT.print(pulseCount);
    SerialBT.print(" | angle: "); SerialBT.println(angle);


    lastPrint = millis();
  }
  // === Serial command check ===
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    if (cmd == 'f') {
      digitalWrite(dir1, HIGH);   // forward
      analogWrite(pwm1, duty);
      Serial.println("forward");
    }
    else if (cmd == 'b') {
      digitalWrite(dir1, LOW);    // backward
      analogWrite(pwm1, duty);
      Serial.println("backward");
    }
    else if (cmd == 's') {
      analogWrite(pwm1, 0);       // stop
      Serial.println("stopped");
    }
  }
}