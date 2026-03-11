#include <Arduino.h>
#include <FastInterruptEncoder.h>
#include <BluetoothSerial.h>
#include <ArduinoJson.h>

/* ===================== BLUETOOTH ===================== */
BluetoothSerial SerialBT;
#define ESP_ID 1

/* ===================== OPTICAL ENCODER ===================== */
#define OPT_A 25
#define OPT_B 26

Encoder optEnc(OPT_A, OPT_B, SINGLE);
const float OPT_CPR = 232464.0/ 4.0;   // already full decode
const float OPT_GEAR = 30.0;

/* ===================== MAGNETIC QUADRATURE ===================== */
#define MAG_A 21
#define MAG_B 19

volatile long magCount = 0;
volatile uint8_t magLastState = 0;
const float MAG_CPR = 9048.0;
const float MAG_GEAR = 30.0;

void IRAM_ATTR magneticISR() {
  uint8_t a = digitalRead(MAG_A);
  uint8_t b = digitalRead(MAG_B);
  uint8_t state = (a << 1) | b;
  uint8_t transition = (magLastState << 2) | state;

  switch (transition) {
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
      magCount++;
      break;

    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
      magCount--;
      break;
  }
  magLastState = state;
}

/* ===================== AS5600 ===================== */
#define AS5600_OUT 34

/* ===================== GLOBALS ===================== */
float angles[3] = {0, 0, 0};
unsigned long lastSend = 0;
const unsigned long SEND_PERIOD = 20; // 50 Hz

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_ENC_A");

  optEnc.init();

  pinMode(MAG_A, INPUT_PULLUP);
  pinMode(MAG_B, INPUT_PULLUP);
  magLastState = (digitalRead(MAG_A) << 1) | digitalRead(MAG_B);
  attachInterrupt(digitalPinToInterrupt(MAG_A), magneticISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG_B), magneticISR, CHANGE);

  analogReadResolution(12);

  Serial.println("✔ ESP-A encoders ready");
}

/* ===================== LOOP ===================== */
void loop() {

  // REQUIRED for FastInterruptEncoder  
  optEnc.loop();

  /* ---------- Optical ---------- */
  long optCount = optEnc.getTicks();
  angles[0] = (optCount * 360.0) / (OPT_CPR * OPT_GEAR);

  /* ---------- Magnetic ---------- */
  noInterrupts();
  long magSnapshot = magCount;
  interrupts();
  angles[1] = (magSnapshot * 360.0) / (MAG_CPR * MAG_GEAR);

  /* ---------- AS5600 ---------- */
  int adc = analogRead(AS5600_OUT);
  angles[2] = (adc * 360.0) / 4095.0;

  /* ---------- Bluetooth JSON ---------- */
  if (millis() - lastSend >= SEND_PERIOD) {

    StaticJsonDocument<128> doc;

    doc["id"] = ESP_ID;
    JsonArray arr = doc.to<JsonArray>();
    //JsonArray arr = doc.createNestedArray("angles");

    for (int i = 0; i < 3; i++){
      arr.add(angles[i]);
    }

    String out;
    serializeJson(doc, out);

    SerialBT.println(out);
    Serial.println(out);

    lastSend = millis();
  }
}
