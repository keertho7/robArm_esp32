#include <Arduino.h>
#include <FastInterruptEncoder.h>
#include <BluetoothSerial.h>
#include <ArduinoJson.h>

/* ===================== BLUETOOTH ===================== */
BluetoothSerial SerialBT;

/* ===================== MOTOR PINS ===================== */
// Local ID 0: Elbow
#define M0_DIR 27
#define M0_PWM 14

// Local ID 1: Shoulder 1
#define M1_DIR 22
#define M1_PWM 23

// Local ID 2: Shoulder 2
#define M2_DIR 17
#define M2_PWM 18

struct MotorPin {
  int dir;
  int pwm;
};

MotorPin motorPins[3] = {
  {M0_DIR, M0_PWM},
  {M1_DIR, M1_PWM},
  {M2_DIR, M2_PWM}
};

void setMotor(uint8_t id, bool fwd, uint8_t pwm) {
  if (id > 2) return;
  digitalWrite(motorPins[id].dir, fwd ? HIGH : LOW);
  analogWrite(motorPins[id].pwm, pwm);
}

/* ===================== OPTICAL ENCODER (SHOULDER 2) ===================== */
#define OPT_A 4
#define OPT_B 16

Encoder optEnc(OPT_A, OPT_B, SINGLE, 200);
const float OPT_CPR  = 232464.0 / 4.0;
const float OPT_GEAR = 30.0;

/* ===================== MAGNETIC ENCODER (SHOULDER 1) ===================== */
#define MAG_A 21
#define MAG_B 19

volatile long magCount = 0;
volatile uint8_t magLastState = 0;

const float MAG_CPR  = 9048.0;
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
#define AS5600_ELBOW    34
#define AS5600_SHOULDER 35

/* ===================== GLOBALS ===================== */
float angles[4] = {0, 0, 0, 0};
unsigned long lastSend = 0;
const unsigned long SEND_PERIOD = 20; // 50 Hz

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_B_ARM");

  for (int i = 0; i < 3; i++) {
    pinMode(motorPins[i].dir, OUTPUT);
    pinMode(motorPins[i].pwm, OUTPUT);
    digitalWrite(motorPins[i].dir, LOW);
    analogWrite(motorPins[i].pwm, 0);
  }

  if (!optEnc.init()) {
    Serial.println("Optical encoder FAILED");
    while (1);
  }

  pinMode(MAG_A, INPUT_PULLUP);
  pinMode(MAG_B, INPUT_PULLUP);
  magLastState = (digitalRead(MAG_A) << 1) | digitalRead(MAG_B);
  attachInterrupt(digitalPinToInterrupt(MAG_A), magneticISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG_B), magneticISR, CHANGE);

  analogReadResolution(12);

  Serial.println("✔ ESP-B motors + encoders ready");
}

/* ===================== LOOP ===================== */
void loop() {

  /* ---------- MOTOR RX ---------- */
  while (SerialBT.available()) {
    uint8_t b = SerialBT.read();
    if (b != 0xAA) continue;

    while (SerialBT.available() < 3);

    uint8_t id  = SerialBT.read();
    uint8_t dir = SerialBT.read();
    uint8_t pwm = SerialBT.read();

    setMotor(id, dir, pwm);
  }

  /* ---------- ENCODERS ---------- */
  optEnc.loop();

  int adcElbow = analogRead(AS5600_ELBOW);
  angles[0] = (adcElbow * 360.0) / 4095.0;

  noInterrupts();
  long magSnap = magCount;
  interrupts();
  angles[1] = (magSnap * 360.0) / (MAG_CPR * MAG_GEAR);

  long opt = optEnc.getTicks();
  angles[2] = (opt * 360.0) / (OPT_CPR * OPT_GEAR);

  int adcShoulder = analogRead(AS5600_SHOULDER);
  angles[3] = (adcShoulder * 360.0) / 4095.0;

  /* ---------- BLUETOOTH JSON TX ---------- */
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