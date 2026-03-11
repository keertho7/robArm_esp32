#include <Arduino.h>
#include <BluetoothSerial.h> 

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

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32B_ARM"); //change SerialBT to just Serial

  for (int i = 0; i < 3; i++) {
    pinMode(motorPins[i].dir, OUTPUT);
    pinMode(motorPins[i].pwm, OUTPUT);
    digitalWrite(motorPins[i].dir, LOW);
    analogWrite(motorPins[i].pwm, 0);
  }

  Serial.println("ESPB motors ready");
}

/* ===================== LOOP ===================== */
void loop() {

  /* ---------- MOTOR RX ---------- */
  while (SerialBT.available()) {   //change SerialBT to just Serial
    uint8_t b = SerialBT.read();   //change SerialBT to just Serial
    if (b != 0xAA) continue;

    while (SerialBT.available() < 3);   //change SerialBT to just Serial

    uint8_t id  = SerialBT.read();  //change SerialBT to just Serial
    uint8_t dir = SerialBT.read();  //change SerialBT to just Serial
    uint8_t pwm = SerialBT.read();  //change SerialBT to just Serial

    setMotor(id, dir, pwm);
  }
}