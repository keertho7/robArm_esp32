#include <Arduino.h>
//#include <FastInterruptEncoder.h>
#include <BluetoothSerial.h>
#include <ArduinoJson.h>

/* ===================== BLUETOOTH ===================== */
BluetoothSerial SerialBT;

/* ===================== MOTOR PINS ===================== */
// Local ID 0: Gripper
#define M0_DIR 32
#define M0_PWM 33

// Local ID 1: Wrist1
#define M1_DIR 27
#define M1_PWM 14

// Local ID 2: Wrist2
#define M2_DIR 22
#define M2_PWM 23

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

/* ===================== MAGNETIC ENCODER – ELBOW ===================== */
#define MAG2_A 4
#define MAG2_B 16

volatile long mag2Count = 0;
volatile uint8_t mag2LastState = 0;

/* ===================== MAGNETIC – WRIST1 ===================== */
#define MAG0_A 25
#define MAG0_B 26

volatile long mag0Count = 0;
volatile uint8_t mag0LastState = 0;

/* ===================== MAGNETIC – WRIST2 ===================== */
#define MAG1_A 21
#define MAG1_B 19

volatile long mag1Count = 0;
volatile uint8_t mag1LastState = 0;

const float MAG_CPR   = 9048.0;
const float MAG_GEAR1 = 1.0;
const float MAG_GEAR2 = 30.0;

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

/* ===================== MAG ISR – ELBOW ===================== */
void IRAM_ATTR mag2ISR() {
  uint8_t a = digitalRead(MAG2_A);
  uint8_t b = digitalRead(MAG2_B);
  uint8_t state = (a << 1) | b;
  uint8_t transition = (mag2LastState << 2) | state;

  switch (transition) {
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
      mag2Count++;
      break;
    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
      mag2Count--;
      break;
  }
  mag2LastState = state;
}

/* ===================== GLOBALS ===================== */
float angles[3] = {0, 0, 0};
unsigned long lastSend = 0;
const unsigned long SEND_PERIOD = 50;

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_A_ARM");

  for (int i = 0; i < 3; i++) {
    pinMode(motorPins[i].dir, OUTPUT);
    pinMode(motorPins[i].pwm, OUTPUT);
    digitalWrite(motorPins[i].dir, LOW);
    analogWrite(motorPins[i].pwm, 0);
  }

  pinMode(MAG0_A, INPUT_PULLUP);
  pinMode(MAG0_B, INPUT_PULLUP);
  pinMode(MAG1_A, INPUT_PULLUP);
  pinMode(MAG1_B, INPUT_PULLUP);
  pinMode(MAG2_A, INPUT_PULLUP);
  pinMode(MAG2_B, INPUT_PULLUP);

  mag0LastState = (digitalRead(MAG0_A) << 1) | digitalRead(MAG0_B);
  mag1LastState = (digitalRead(MAG1_A) << 1) | digitalRead(MAG1_B);
  mag2LastState = (digitalRead(MAG2_A) << 1) | digitalRead(MAG2_B);

  attachInterrupt(digitalPinToInterrupt(MAG0_A), mag0ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG0_B), mag0ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG1_A), mag1ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG1_B), mag1ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG2_A), mag2ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG2_B), mag2ISR, CHANGE);

  Serial.println("✔ ESP-A motors + magnetic encoders ready");
}

/* ===================== LOOP ===================== */
void loop() {

  /* ---------- MOTOR RX ---------- */
  /*while (SerialBT.available() >= 3) {
    uint8_t id  = SerialBT.read();
    uint8_t dir = SerialBT.read();
    uint8_t pwm = SerialBT.read();
    Serial.print("RX | ID: ");
    Serial.print(id);
    Serial.print(" DIR: ");
    Serial.print(dir);
    Serial.print(" PWM: ");
    Serial.println(pwm);

    setMotor(id, dir, pwm);
  }*/
 while (SerialBT.available()) {
  uint8_t b = SerialBT.read();

  if (b != 0xAA) {
    continue; // not a sync byte, ignore
  }

  // Wait until full packet is available
  while (SerialBT.available() < 3);

  uint8_t id  = SerialBT.read();
  uint8_t dir = SerialBT.read();
  uint8_t pwm = SerialBT.read();

  setMotor(id, dir, pwm);

  // Optional debug
  Serial.print("RX | ID: ");
  Serial.print(id);
  Serial.print(" DIR: ");
  Serial.print(dir);
  Serial.print(" PWM: ");
  Serial.println(pwm);
}


  /* ---------- ENCODERS ---------- */
  noInterrupts();
  long m0 = mag0Count;
  long m1 = mag1Count;
  long m2 = mag2Count;
  interrupts();

  angles[0] = (m0 * 360.0) / (MAG_CPR * MAG_GEAR1);
  angles[1] = (m1 * 360.0) / (MAG_CPR * MAG_GEAR1);
  angles[2] = (m2 * 360.0) / (MAG_CPR * MAG_GEAR2);

  /* ---------- BLUETOOTH JSON TX ---------- */
  if (millis() - lastSend >= SEND_PERIOD) {
    StaticJsonDocument<96> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < 3; i++) arr.add(angles[i]);

    String out;
    serializeJson(arr, out);
    SerialBT.println(out);
    //Serial.println(out);
    lastSend = millis();
  }
}
