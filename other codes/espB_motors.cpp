#include <Arduino.h>
#include <BluetoothSerial.h>

/* ===================== BLUETOOTH ===================== */
BluetoothSerial SerialBT;

/* ===================== MOTOR PINS ===================== */
// Local ID 0: Elbow
#define M0_DIR 26
#define M0_PWM 25

// Local ID 1: Shoulder1
#define M1_DIR 27
#define M1_PWM 14

// Local ID 2: Shoulder2
#define M2_DIR 21
#define M2_PWM 13

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
  digitalWrite(motorPins[id].dir, fwd ? HIGH : LOW);
  analogWrite(motorPins[id].pwm, pwm);
}

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Motor_ESP_B");

  for (int i = 0; i < 3; i++) {
    pinMode(motorPins[i].dir, OUTPUT);
    pinMode(motorPins[i].pwm, OUTPUT);
    digitalWrite(motorPins[i].dir, LOW);
    analogWrite(motorPins[i].pwm, 0);
  }

  Serial.println("✔ ESP-B Motor Controller Ready");
}

/* ===================== LOOP ===================== */
void loop() {
  while (SerialBT.available() >= 3) {
    uint8_t id  = SerialBT.read();
    uint8_t dir = SerialBT.read();
    uint8_t pwm = SerialBT.read();

    if (id > 2) continue;

    setMotor(id, dir, pwm);
    Serial.printf("[ESP-B] M%d: %s PWM=%d\n",
                  id, dir ? "FWD" : "REV", pwm);
  }
}
