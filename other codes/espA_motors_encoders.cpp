#include <Arduino.h>
#include <FastInterruptEncoder.h>
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

/* ===================== magnetic ENCODER (ELBOW) ===================== */
#define MAG2_A 4
#define MAG2_B 16

//Encoder optEnc(OPT_A, OPT_B, SINGLE);
//const float OPT_CPR  = 232464.0 / 4.0;
//const float OPT_GEAR = 30.0;
volatile long mag2Count = 0;
volatile uint8_t mag2LastState = 0;

/* ===================== MAGNETIC – WRIST1 ===================== */
#define MAG0_A 36 //26
#define MAG0_B 39 //25

volatile long mag0Count = 0;
volatile uint8_t mag0LastState = 0;

/* ===================== MAGNETIC – WRIST2 ===================== */
#define MAG1_A 21
#define MAG1_B 19

volatile long mag1Count = 0;
volatile uint8_t mag1LastState = 0;

const float MAG_CPR  = 9048.0;
const float MAG_GEAR1 = 1.0;
const float MAG_GEAR2 = 30.0;



/* ===================== LIMIT SWITCHES (NEW) ===================== */
#define LIMIT_UP    35
#define LIMIT_DOWN  34

/* ===================== WRIST HOMING STATE (NEW) ===================== */
bool wristHomed = false;
bool wristBackingOff = false;
unsigned long backoffStart = 0;
const unsigned long BACKOFF_TIME = 150; // ms
bool homingRequested = false;   // NEW
int wristDir = 0; // -1 down, +1 up, 0 idle


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

/* ===================== MAG ISR – elbow ===================== */
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
const unsigned long SEND_PERIOD = 20; // 50 Hz

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_A_ARM");

  /* Motor pins */
  for (int i = 0; i < 3; i++) {
    pinMode(motorPins[i].dir, OUTPUT);
    pinMode(motorPins[i].pwm, OUTPUT);
    digitalWrite(motorPins[i].dir, LOW);
    analogWrite(motorPins[i].pwm, 0);
  }

  /* Optical encoder */
  //optEnc.init();

  /* Magnetic encoders */
  pinMode(MAG0_A, INPUT_PULLUP);
  pinMode(MAG0_B, INPUT_PULLUP);
  pinMode(MAG1_A, INPUT_PULLUP);
  pinMode(MAG1_B, INPUT_PULLUP);
  pinMode(MAG2_A, INPUT_PULLUP);
  pinMode(MAG2_B, INPUT_PULLUP);
    /* ---------- Limit switches (NEW) ---------- */
  pinMode(LIMIT_UP, INPUT_PULLUP);
  pinMode(LIMIT_DOWN, INPUT_PULLUP);


  mag0LastState = (digitalRead(MAG0_A) << 1) | digitalRead(MAG0_B);
  mag1LastState = (digitalRead(MAG1_A) << 1) | digitalRead(MAG1_B);
  mag2LastState = (digitalRead(MAG2_A) << 1) | digitalRead(MAG2_B);

  attachInterrupt(digitalPinToInterrupt(MAG0_A), mag0ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG0_B), mag0ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG1_A), mag1ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG1_B), mag1ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG2_A), mag2ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(MAG2_B), mag2ISR, CHANGE);

  Serial.println("✔ ESP-A motors + encoders ready");
}

/* ===================== LOOP ===================== */
void loop() {

  /* ---------- MOTOR RX (binary) ---------- */
  while (SerialBT.available() >= 3) {
    uint8_t id  = SerialBT.read();
    uint8_t dir = SerialBT.read();
    uint8_t pwm = SerialBT.read();

    /* ---------- Manual homing trigger (NEW) ---------- */
  if (id == 0xFF) {           // special HOME command
    wristHomed = false;
    wristBackingOff = false;
    homingRequested = true;   // NEW
    Serial.println("HOMING REQUESTED");
    continue;
  }

  /* ---------- Block wrist motion until homed (NEW) ---------- */ //REMOVE THIS COMMENT AFTER LIMIT SWITCHES ARE ADDED
  //if ((id == 1 || id == 2) && !wristHomed) {
    //continue;
  //}

    /* ---------- Track wrist direction (NEW) ---------- */
  if (id == 1 || id == 2) {
    wristDir = dir ? +1 : -1;
  }

    setMotor(id, dir, pwm);    
  }

  /* ===================== WRIST SAFETY + HOMING (NEW) ===================== */

bool upperHit = digitalRead(LIMIT_UP) == LOW;
bool lowerHit = digitalRead(LIMIT_DOWN) == LOW;

/* ---------- Homing ---------- */
if (!wristHomed && homingRequested) {
  // Move wrist UP slowly
  digitalWrite(M1_DIR, LOW);
  digitalWrite(M2_DIR, HIGH);
  analogWrite(M1_PWM, 80);
  analogWrite(M2_PWM, 80);

  if (upperHit) {
    // Stop
    analogWrite(M1_PWM, 0);
    analogWrite(M2_PWM, 0);

    // ZERO encoder reference
    mag0Count = 0;

    // Start backoff
    backoffStart = millis();
    wristBackingOff = true;
    wristHomed = true;
    homingRequested = false;  //new
  }
}

/* ---------- Backoff ---------- */
if (wristBackingOff) {
  digitalWrite(M1_DIR, HIGH);
  digitalWrite(M2_DIR, LOW);
  analogWrite(M1_PWM, 80);
  analogWrite(M2_PWM, 80);

  if (millis() - backoffStart > BACKOFF_TIME) {
    analogWrite(M1_PWM, 0);
    analogWrite(M2_PWM, 0);
    wristBackingOff = false;
  }
}

/* ---------- Runtime safety (DIRECTION AWARE) ---------- */
if (wristHomed) {
  if (upperHit && wristDir == +1) {
    analogWrite(M1_PWM, 0);
    analogWrite(M2_PWM, 0);
    wristDir = 0;
  }
  if (lowerHit && wristDir == -1) {
    analogWrite(M1_PWM, 0);
    analogWrite(M2_PWM, 0);
    wristDir = 0;
  }
}


  /* ---------- ENCODERS ---------- */
  //optEnc.loop();

  noInterrupts();
  long m0 = mag0Count;
  long m1 = mag1Count;
  long m2 = mag2Count;
  interrupts();

  angles[0] = (m0 * 360.0) / (MAG_CPR * MAG_GEAR1);  // wrist1
  angles[1] = (m1 * 360.0) / (MAG_CPR * MAG_GEAR1);  // wrist2
  angles[2] = (m2 * 360.0) / (MAG_CPR * MAG_GEAR2);  // elbow
  //long opt = optEnc.getTicks();
  //angles[2] = (opt * 360.0) / (OPT_CPR * OPT_GEAR); // elbow

  /* ---------- BLUETOOTH JSON TX ---------- */
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
