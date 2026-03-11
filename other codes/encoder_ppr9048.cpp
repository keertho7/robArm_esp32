#include <Arduino.h>

#define ENC_A 16
#define ENC_B 17

volatile long count = 0;
volatile int direction = 0;

volatile uint8_t lastState = 0;

unsigned long lastTime = 0;
long lastCount = 0;

const float CPR = 9048.0;

void IRAM_ATTR encoderISR() {
  uint8_t a = digitalRead(ENC_A);
  uint8_t b = digitalRead(ENC_B);

  uint8_t state = (a << 1) | b;
  uint8_t transition = (lastState << 2) | state;

  // Valid quadrature transitions
  switch (transition) {
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
      count++;
      direction = +1;
      break;

    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
      count--;
      direction = -1;
      break;
  }

  lastState = state;
}

void setup() {
  Serial.begin(115200);

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  lastState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);

  attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), encoderISR, CHANGE);

  lastTime = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastTime >= 100) {

    long currentCount;
    noInterrupts();
    currentCount = count;
    interrupts();

    long delta = currentCount - lastCount;
    float rpm = (delta * 600.0) / CPR;

    lastCount = currentCount;
    lastTime = now;

    Serial.print("Count: ");
    Serial.print(currentCount);

    Serial.print(" | RPM: ");
    Serial.print(rpm);

    Serial.print(" | Dir: ");
    Serial.println(direction == 1 ? "CW" : "CCW");
  }
}