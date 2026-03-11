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

// Local ID 1: Shoulder right
#define M1_DIR 22
#define M1_PWM 23

// Local ID 2: Shoulder left
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

/* ===================== OPTICAL ENCODER (SHOULDER LEFT) ===================== */
#define OPT_A 21
#define OPT_B  19

Encoder optEnc(OPT_A, OPT_B, SINGLE, 200);
const float OPT_CPR  = 232464.0 / 4.0;
const float OPT_GEAR = 30.0;

/* ===================== MAGNETIC ENCODER (SHOULDER RIGHT) ===================== */
#define MAG_SHR_A 4
#define MAG_SHR_B 16

volatile long magShrCount = 0;
volatile uint8_t magShrLast = 0;

/* ===================== MAGNETIC ENCODER (ELBOW) ===================== */
#define MAG_ELB_A 26
#define MAG_ELB_B 25

volatile long magElbCount = 0;
volatile uint8_t magElbLast = 0;

const float MAG_CPR  = 9048.0;
const float MAG_GEAR = 30.0;

/* ===================== MAG ISR HELPER ===================== */
inline void handleMagISR(
  volatile long &count,
  volatile uint8_t &last,
  uint8_t A,
  uint8_t B
) {
  uint8_t a = digitalRead(A);
  uint8_t b = digitalRead(B);
  uint8_t state = (a << 1) | b;
  uint8_t transition = (last << 2) | state;

  switch (transition) {
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000: count++; break;
    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011: count--; break;
  }

  last = state;
}

void IRAM_ATTR magShrISR() {
  handleMagISR(magShrCount, magShrLast, MAG_SHR_A, MAG_SHR_B);
}

void IRAM_ATTR magElbISR() {
  handleMagISR(magElbCount, magElbLast, MAG_ELB_A, MAG_ELB_B);
}

/* ===================== GLOBALS ===================== */
float angles[3] = {0, 0, 0};
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

  /* Optical encoder */
  if (!optEnc.init()) {
    Serial.println("Optical encoder FAILED");
    while (1);
  }

  /* Magnetic encoders */
  pinMode(MAG_SHR_A, INPUT_PULLUP);
  pinMode(MAG_SHR_B, INPUT_PULLUP);
  pinMode(MAG_ELB_A, INPUT_PULLUP);
  pinMode(MAG_ELB_B, INPUT_PULLUP);

  magShrLast = (digitalRead(MAG_SHR_A) << 1) | digitalRead(MAG_SHR_B);
  magElbLast = (digitalRead(MAG_ELB_A) << 1) | digitalRead(MAG_ELB_B);

  attachInterrupt(digitalPinToInterrupt(MAG_SHR_A), magShrISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG_SHR_B), magShrISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG_ELB_A), magElbISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG_ELB_B), magElbISR, CHANGE);

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

  noInterrupts();
  long elb = magElbCount;
  long shr = magShrCount;
  interrupts();

  long shl = optEnc.getTicks();

  angles[0] = (elb * 360.0) / (MAG_CPR * MAG_GEAR); // elbow
  angles[1] = (shr * 360.0) / (MAG_CPR * MAG_GEAR); // shoulder right
  angles[2] = (shl * 360.0) / (OPT_CPR * OPT_GEAR); // shoulder left

  /* ---------- BLUETOOTH JSON TX ---------- */
  if (millis() - lastSend >= SEND_PERIOD) {
    StaticJsonDocument<96> doc;
    JsonArray arr = doc.to<JsonArray>();

    arr.add(angles[0]);
    arr.add(angles[1]);
    arr.add(angles[2]);

    String out;
    serializeJson(arr, out);
    SerialBT.println(out);

    lastSend = millis();
  }
}
