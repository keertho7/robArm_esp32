#include <Arduino.h>
#include <FastInterruptEncoder.h>
#include <BluetoothSerial.h>
#include <ArduinoJson.h>

/* ===================== BLUETOOTH ===================== */
BluetoothSerial SerialBT;
/* ===================== OPTICAL ENCODER (ELBOW) ===================== */
#define OPT_A 25
#define OPT_B 26

Encoder optEnc(OPT_A, OPT_B, SINGLE);
const float OPT_CPR  = 232464.0 / 4.0;
const float OPT_GEAR = 30.0;

/* ===================== MAGNETIC – WRIST1 ===================== */
#define MAG0_A 21
#define MAG0_B 19

volatile long mag0Count = 0;
volatile uint8_t mag0LastState = 0;

/* ===================== MAGNETIC – WRIST2 ===================== */
#define MAG1_A 32
#define MAG1_B 33

volatile long mag1Count = 0;
volatile uint8_t mag1LastState = 0;

const float MAG_CPR  = 9048.0;
const float MAG_GEAR = 30.0;

/* ===================== MAG ISR – WRIST1 ===================== */
void IRAM_ATTR mag0ISR() {
  uint8_t a = digitalRead(MAG0_A);
  uint8_t b = digitalRead(MAG0_B);
  uint8_t state = (a << 1) | b;
  uint8_t transition = (mag0LastState << 2) | state;

  switch (transition) {
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
      mag0Count++;
      break;

    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
      mag0Count--;
      break;
  }

  mag0LastState = state;
}

/* ===================== MAG ISR – WRIST2 ===================== */
void IRAM_ATTR mag1ISR() {
  uint8_t a = digitalRead(MAG1_A);
  uint8_t b = digitalRead(MAG1_B);
  uint8_t state = (a << 1) | b;
  uint8_t transition = (mag1LastState << 2) | state;

  switch (transition) {
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
      mag1Count++;
      break;

    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
      mag1Count--;
      break;
  }

  mag1LastState = state;
}

/* ===================== GLOBALS ===================== */
float angles[3] = {0, 0, 0};
unsigned long lastSend = 0;
const unsigned long SEND_PERIOD = 20; // 50 Hz

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_ENC_A");

  /* Optical encoder */
  optEnc.init();

  /* Magnetic encoders */
  pinMode(MAG0_A, INPUT_PULLUP);
  pinMode(MAG0_B, INPUT_PULLUP);
  pinMode(MAG1_A, INPUT_PULLUP);
  pinMode(MAG1_B, INPUT_PULLUP);

  mag0LastState = (digitalRead(MAG0_A) << 1) | digitalRead(MAG0_B);
  mag1LastState = (digitalRead(MAG1_A) << 1) | digitalRead(MAG1_B);

  attachInterrupt(digitalPinToInterrupt(MAG0_A), mag0ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG0_B), mag0ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG1_A), mag1ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG1_B), mag1ISR, CHANGE);

  Serial.println("✔ ESP-A encoders ready");
}

/* ===================== LOOP ===================== */
void loop() {

  /* REQUIRED for FastInterruptEncoder */
  optEnc.loop();

  /* Snapshot magnetic counts */
  noInterrupts();
  long m0 = mag0Count;
  long m1 = mag1Count;
  interrupts();

  /* Wrist1 – magnetic */
  angles[0] = (m0 * 360.0) / (MAG_CPR * MAG_GEAR);

  /* Wrist2 – magnetic */
  angles[1] = (m1 * 360.0) / (MAG_CPR * MAG_GEAR);

  /* Elbow – optical */
  long opt = optEnc.getTicks();
  angles[2] = (opt * 360.0) / (OPT_CPR * OPT_GEAR);

  /* ---------- Bluetooth JSON ---------- */
  if (millis() - lastSend >= SEND_PERIOD) {

    StaticJsonDocument<96> doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < 3; i++) {
      arr.add(angles[i]);
    }

    String out;
    serializeJson(arr, out);

    SerialBT.println(out);
    Serial.println(out);

    lastSend = millis();
  }
}
