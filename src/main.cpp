/**
 * RC Car - Motor Test
 * Arduino Uno + IRF3205 MOSFETs
 *
 * Wiring per motor (low-side switch, forward only for now):
 *   Motor(+) ---> 12V
 *   Motor(-) ---> IRF3205 Drain
 *   IRF3205 Source --> GND (common with Arduino GND)
 *   IRF3205 Gate  --> 100Ω --> PWM Pin
 *                          +-- 10kΩ --> GND (pull-down)
 *
 * PIN ASSIGNMENTS (2 pins per motor: pinA=forward, pinB=reverse):
 *   Motor FL : pinA=D3,  pinB=D2
 *   Motor FR : pinA=D5,  pinB=D4
 *   Motor RL : pinA=D6,  pinB=D7
 *   Motor RR : pinA=D9,  pinB=D8
 *
 * NOTE: Reverse requires an H-bridge. For now, pinB does nothing.
 *       Physically swap motor wires to test reverse direction manually.
 */

#include <Arduino.h>

// --- Types ---
typedef enum {
  FORWARD,
  REVERSE,
  BRAKE,
  COAST
} MotorDir;

typedef struct {
  uint8_t pinA;  // forward
  uint8_t pinB;  // reverse
} Motor;

// --- Motor Definitions ---
const Motor MOTORS[] = {
  {3, 2},   // FL
  {5, 4},   // FR
  {6, 7},   // RL
  {9, 8}    // RR
};
const uint8_t NUM_MOTORS = 4;

// --- Forward Declarations ---
void driveMotor(Motor m, MotorDir dir, uint8_t speed);
void driveAll(MotorDir dir, uint8_t speed);
void stopAll();
void brakeAll();

// --- Core Motor Control ---

void driveMotor(Motor m, MotorDir dir, uint8_t speed) {
  switch (dir) {
    case FORWARD:
      analogWrite(m.pinA, speed);
      digitalWrite(m.pinB, LOW);
      break;
    case REVERSE:
      digitalWrite(m.pinA, LOW);
      analogWrite(m.pinB, speed);
      break;
    case BRAKE:
      digitalWrite(m.pinA, LOW);
      digitalWrite(m.pinB, LOW);
      break;
    case COAST:
    default:
      digitalWrite(m.pinA, LOW);
      digitalWrite(m.pinB, LOW);
      break;
  }
}

void driveAll(MotorDir dir, uint8_t speed) {
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    driveMotor(MOTORS[i], dir, speed);
  }
}

void stopAll() {
  driveAll(COAST, 0);
}

void brakeAll() {
  driveAll(BRAKE, 0);
}

// --- Test Routines ---

void rampTest(MotorDir dir, const char* label) {
  Serial.print("[TEST] Ramping ");
  Serial.println(label);

  for (int spd = 0; spd <= 255; spd += 5) {
    driveAll(dir, spd);
    delay(30);
  }

  Serial.println("[TEST] Holding full speed for 2s...");
  delay(2000);

  for (int spd = 255; spd >= 0; spd -= 5) {
    driveAll(dir, spd);
    delay(30);
  }

  stopAll();
  Serial.println("[TEST] Done. Motors stopped.");
}

void rampForward() { rampTest(FORWARD, "FORWARD"); }
void rampReverse() { rampTest(REVERSE, "REVERSE"); }

void directionCycleTest() {
  Serial.println("[TEST] Direction cycle: FWD 2s -> brake -> REV 2s -> brake");

  driveAll(FORWARD, 128);
  delay(2000);
  brakeAll();
  delay(500);

  driveAll(REVERSE, 128);
  delay(2000);
  brakeAll();
  delay(500);

  stopAll();
  Serial.println("[TEST] Cycle complete.");
}

// --- Serial Command Handler ---

void handleSerial() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  switch (cmd) {
    case 'f':
      rampForward();
      break;
    case 'r':
      rampReverse();
      break;
    case 'd':
      directionCycleTest();
      break;
    case 's':
      stopAll();
      Serial.println("[CMD] Stopped.");
      break;
    case '0' ... '9': {
      uint8_t percent = (cmd == '0') ? 100 : (cmd - '0') * 10;
      uint8_t speed = (uint8_t)(percent * 255 / 100);
      driveAll(FORWARD, speed);
      Serial.print("[CMD] Forward speed: ");
      Serial.print(percent);
      Serial.print("% (");
      Serial.print(speed);
      Serial.println("/255)");
      break;
    }
    default:
      Serial.println("[CMD] Unknown. Commands: f=fwd ramp, r=rev ramp, d=dir cycle, s=stop, 0-9=speed");
      break;
  }
}

// --- Arduino Lifecycle ---

void setup() {
  Serial.begin(9600);
  Serial.println("=== RC Car Motor Test ===");
  Serial.println("Commands: f=fwd, r=rev, d=dir cycle, s=stop, 0-9=speed");

  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    pinMode(MOTORS[i].pinA, OUTPUT);
    pinMode(MOTORS[i].pinB, OUTPUT);
    digitalWrite(MOTORS[i].pinA, LOW);
    digitalWrite(MOTORS[i].pinB, LOW);
  }

  Serial.println("Ready.");
}

void loop() {
  handleSerial();
}