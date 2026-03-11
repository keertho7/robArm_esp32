#include <Arduino.h>
#include <BluetoothSerial.h>

/* ===================== BLUETOOTH ===================== */
BluetoothSerial SerialBT;

/* ===================== MOTOR PINS (Single-channel drivers: DIR + PWM per motor) ===================== */
// Joint 0: Turntable
#define TT_DIR 21
#define TT_PWM 13
// Joint 1: Shoulder  
#define SH_DIR 27
#define SH_PWM 14

// Joint 2: Elbow
#define EL_DIR 26
#define EL_PWM 25

// Joint 3: Wrist1
#define W1_DIR 33
#define W1_PWM 32

// Joint 4: Wrist2
#define W2_DIR 17
#define W2_PWM 18

// Joint 5: Gripper
#define GR_DIR 19
#define GR_PWM 21

/* ===================== MOTOR CONTROL ===================== */
struct MotorPin {
  int dir, pwm;
};

MotorPin motorPins[6] = {
  {TT_DIR, TT_PWM},  // 0: tt
  {SH_DIR, SH_PWM},  // 1: shoulder
  {EL_DIR, EL_PWM},  // 2: elbow
  {W1_DIR, W1_PWM},  // 3: wrist1
  {W2_DIR, W2_PWM},  // 4: wrist2
  {GR_DIR, GR_PWM}   // 5: gripper
};

void setMotor(int id, bool fwd, uint8_t pwm) {
  digitalWrite(motorPins[id].dir, fwd ? HIGH : LOW);
  analogWrite(motorPins[id].pwm, pwm);  // 0-255
}

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_MotorArm");

  // All motor pins output
  for (int i = 0; i < 6; i++) {
    pinMode(motorPins[i].dir, OUTPUT);
    pinMode(motorPins[i].pwm, OUTPUT);
    digitalWrite(motorPins[i].dir, LOW);
    analogWrite(motorPins[i].pwm, 0);
  }

  Serial.println("✔ Single-channel Motor ESP32 ready - Pair & rfcomm0");
}

/* ===================== LOOP ===================== */
void loop() {
  while (SerialBT.available() >= 3) {
    uint8_t id = SerialBT.read();
    if (id > 5) continue;
    
    bool fwd = SerialBT.read();
    uint8_t pwm = SerialBT.read();
    
    setMotor(id, fwd, pwm);
    Serial.printf("M%d: %s PWM=%d\n", id, fwd ? "FWD" : "REV", pwm);
  }
}
