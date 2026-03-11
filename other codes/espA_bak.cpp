#include <Arduino.h>
#include <BluetoothSerial.h> //remove for wired
#include <ArduinoJson.h>

/* ===================== BLUETOOTH ===================== */
BluetoothSerial SerialBT; //remove for wired

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

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32A_ARM"); //remove for wired

  for (int i = 0; i < 3; i++) {
    pinMode(motorPins[i].dir, OUTPUT);
    pinMode(motorPins[i].pwm, OUTPUT);
    digitalWrite(motorPins[i].dir, LOW);
    analogWrite(motorPins[i].pwm, 0);
  }

  Serial.println("ESP-A motors ready");
}

/* ===================== LOOP ===================== */
void loop() {

 while (SerialBT.available()) { //change SerialBT to just Serial
  uint8_t b = SerialBT.read(); //change SerialBT to just Serial

  if (b != 0xAA) {
    continue; // not a sync byte, ignore
  }

  // Wait until full packet is available
  while (SerialBT.available() < 3); //change SerialBT to just Serial

  uint8_t id  = SerialBT.read();//change SerialBT to just Serial
  uint8_t dir = SerialBT.read();//change SerialBT to just Serial
  uint8_t pwm = SerialBT.read();//change SerialBT to just Serial

  setMotor(id, dir, pwm);

  // Optional debug
  Serial.print("RX | ID: ");
  Serial.print(id);
  Serial.print(" DIR: ");
  Serial.print(dir);
  Serial.print(" PWM: ");
  Serial.println(pwm);
  }
}
