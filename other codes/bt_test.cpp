#include <Arduino.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_TEST");
  Serial.println("ESP READY");
}

void loop() {
  if (SerialBT.available()) {
    char c = SerialBT.read();
    Serial.print("RX: ");
    Serial.println(c);
  }
}
