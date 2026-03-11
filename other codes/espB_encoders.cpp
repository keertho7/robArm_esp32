#include <Arduino.h>
#include <FastInterruptEncoder.h>
#include <BluetoothSerial.h>
#include <ArduinoJson.h>

/* ===================== BLUETOOTH ===================== */
BluetoothSerial SerialBT;

/* ===================== OPTICAL – SHOULDER 2 ===================== */
#define OPT_A 25
#define OPT_B 26

Encoder optEnc(OPT_A, OPT_B, SINGLE);
const float OPT_CPR  = 232464.0 / 4.0;
const float OPT_GEAR = 30.0;

/* ===================== MAGNETIC – SHOULDER 1 ===================== */
#define MAG_A 21
#define MAG_B 19

volatile long magCount = 0;
volatile uint8_t magLastState = 0;

const float MAG_CPR  = 9048.0;
const float MAG_GEAR = 30.0;

/* ===================== MAG ISR – SHOULDER 1 ===================== */
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
#define AS5600_ELBOW    34
#define AS5600_SHOULDER 35

/* ===================== GLOBALS ===================== */
float angles[4] = {0, 0, 0, 0};
unsigned long lastSend = 0;
const unsigned long SEND_PERIOD = 20; // 50 Hz

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_ENC_B");

  /* Optical encoder */
  optEnc.init();

  /* Magnetic encoder */
  pinMode(MAG_A, INPUT_PULLUP);
  pinMode(MAG_B, INPUT_PULLUP);
  magLastState = (digitalRead(MAG_A) << 1) | digitalRead(MAG_B);
  attachInterrupt(digitalPinToInterrupt(MAG_A), magneticISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG_B), magneticISR, CHANGE);

  /* AS5600 ADC */
  analogReadResolution(12);

  Serial.println("✔ ESP-B encoders ready (AS5600 + mag + opt)");
}

/* ===================== LOOP ===================== */
void loop() {

  /* REQUIRED for FastInterruptEncoder */
  optEnc.loop();

  /* ---------- Elbow – AS5600 ---------- */
  int adc_elbow = analogRead(AS5600_ELBOW);
  angles[0] = (adc_elbow * 360.0) / 4095.0;

  /* ---------- Shoulder1 – magnetic ---------- */
  noInterrupts();
  long magSnapshot = magCount;
  interrupts();
  angles[1] = (magSnapshot * 360.0) / (MAG_CPR * MAG_GEAR);

  /* ---------- Shoulder2 – optical ---------- */
  long opt = optEnc.getTicks();
  angles[2] = (opt * 360.0) / (OPT_CPR * OPT_GEAR);

  /* ---------- Shoulder – AS5600 ---------- */
  int adc_shoulder = analogRead(AS5600_SHOULDER);
  angles[3] = (adc_shoulder * 360.0) / 4095.0;

  /* ---------- Bluetooth JSON ---------- */
  if (millis() - lastSend >= SEND_PERIOD) {

    StaticJsonDocument<128> doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < 4; i++) {
      arr.add(angles[i]);
    }

    String out;
    serializeJson(arr, out);

    SerialBT.println(out);
    Serial.println(out);

    lastSend = millis();
  }
}
