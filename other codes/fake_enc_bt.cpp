#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

unsigned long lastSend = 0;
int counter = 0;

void setup() {
  Serial.begin(115200);

  SerialBT.begin("ESP32_BT_TEST");  // Bluetooth name
  Serial.println("Bluetooth started");
  Serial.println("ESP ready to send & receive");
}

void loop() {
 
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 100) {
    SerialBT.write('E');   
    Serial.println("Sent: E");
    lastSend = millis();
  }

  
  if (SerialBT.available()) {
    uint8_t rx = SerialBT.read();
    Serial.print("Received from ROS: ");
    Serial.println(rx, HEX);
  }
}
