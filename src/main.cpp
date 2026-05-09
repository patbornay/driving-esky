/**
 * Drisky - Line Follow Test
 * Arduino Uno + L298N Motor Driver (2 motors, EN jumpers on)
 *
 * WIRING:
 *   IN1 → D2  IN2 → D3  (Motor A)
 *   IN3 → D4  IN4 → D5  (Motor B)
 *   ENA/ENB jumpers ON (full speed, no PWM needed)
 *   Common GND between Arduino and L298N
 *
 * IR SENSOR (Duinotech XC4524):
 *   Line sensor OUT → A0
 *   VCC → 5V, GND → GND, EN → leave disconnected
 *   Logic: LOW = line detected, HIGH = off line
 *
 * COMMANDS:
 *   l - start line follow
 *   k - stop
 */

#include <Arduino.h>

// ============================================================
// --- Types ---
// ============================================================

typedef enum {
  FORWARD,
  REVERSE,
  COAST
} MotorDir;

// en=255 means no EN pin (jumper on board, always enabled)
typedef struct {
  uint8_t en;
  uint8_t in1;
  uint8_t in2;
} Motor;

typedef enum {
  LAST_TURN_NONE,
  LAST_TURN_LEFT,
  LAST_TURN_RIGHT
} LastTurn;

// ============================================================
// --- Config ---
// ============================================================

const Motor MOTORS[] = {
  {255, 2, 3},   // Motor A: IN1=D2, IN2=D3
  {255, 4, 5},   // Motor B: IN1=D4, IN2=D5
  {255, 2, 3},   // mirrored A (temp until all 4 wired)
  {255, 4, 5},   // mirrored B (temp until all 4 wired)
};
const uint8_t NUM_MOTORS = 4;

#define MOTOR_FL 0
#define MOTOR_FR 1
#define MOTOR_RL 2
#define MOTOR_RR 3

#define IR_LINE_PIN A0

// Tune these if the shimmy is too wide or too slow
const uint8_t LINE_SPEED      = 200;  // forward speed (EN jumper = full, this unused but kept for later)
const uint8_t LINE_TURN_SPEED = 200;  // turn speed
#define LINE_POLL_MS 50               // how often to check sensor (ms)

// ============================================================
// --- State ---
// ============================================================

bool     lineFollowing = false;
LastTurn lastTurn      = LAST_TURN_NONE;
unsigned long lastLinePoll = 0;

// ============================================================
// --- Forward Declarations ---
// ============================================================

void driveMotor(Motor m, MotorDir dir, uint8_t speed);
void driveAll(MotorDir dir, uint8_t speed);
void stopAll();
void lineFollowStraight();
void lineFollowTurnLeft();
void lineFollowTurnRight();
void tickLineFollow();

// ============================================================
// --- Motor Control ---
// ============================================================

void driveMotor(Motor m, MotorDir dir, uint8_t speed) {
  switch (dir) {
    case FORWARD:
      digitalWrite(m.in1, HIGH);
      digitalWrite(m.in2, LOW);
      if (m.en != 255) analogWrite(m.en, speed);
      break;
    case REVERSE:
      digitalWrite(m.in1, LOW);
      digitalWrite(m.in2, HIGH);
      if (m.en != 255) analogWrite(m.en, speed);
      break;
    case COAST:
    default:
      digitalWrite(m.in1, LOW);
      digitalWrite(m.in2, LOW);
      if (m.en != 255) analogWrite(m.en, 0);
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

// Left wheels reverse, right wheels forward
void lineFollowTurnLeft() {
  driveMotor(MOTORS[MOTOR_FL], REVERSE, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RL], REVERSE, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_FR], FORWARD, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RR], FORWARD, LINE_TURN_SPEED);
  lastTurn = LAST_TURN_LEFT;
}

// Left wheels forward, right wheels reverse
void lineFollowTurnRight() {
  driveMotor(MOTORS[MOTOR_FL], FORWARD, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RL], FORWARD, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_FR], REVERSE, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RR], REVERSE, LINE_TURN_SPEED);
  lastTurn = LAST_TURN_RIGHT;
}

void lineFollowStraight() {
  driveAll(FORWARD, LINE_SPEED);
}

// ============================================================
// --- Line Follow ---
// ============================================================

/**
 * Shimmy logic:
 *   On line  → steer opposite of last sweep to re-centre
 *   Off line → sweep opposite of last steer to find line again
 *
 * Example cycle:
 *   No history  → sweep LEFT
 *   Finds line  → steer RIGHT to centre
 *   Loses line  → sweep LEFT
 *   Finds line  → steer RIGHT ... repeat
 */
void tickLineFollow() {
  if (!lineFollowing) return;

  unsigned long now = millis();
  if (now - lastLinePoll < LINE_POLL_MS) return;
  lastLinePoll = now;

  bool lineDetected = (digitalRead(IR_LINE_PIN) == LOW);

  if (lineDetected) {
    switch (lastTurn) {
      case LAST_TURN_LEFT:
        lineFollowTurnRight();
        Serial.println("[LINE] On line → steering RIGHT");
        break;
      case LAST_TURN_RIGHT:
        lineFollowTurnLeft();
        Serial.println("[LINE] On line → steering LEFT");
        break;
      case LAST_TURN_NONE:
      default:
        lineFollowStraight();
        Serial.println("[LINE] On line → straight");
        break;
    }
  } else {
    switch (lastTurn) {
      case LAST_TURN_RIGHT:
        lineFollowTurnLeft();
        Serial.println("[LINE] Lost line → sweeping LEFT");
        break;
      case LAST_TURN_LEFT:
        lineFollowTurnRight();
        Serial.println("[LINE] Lost line → sweeping RIGHT");
        break;
      case LAST_TURN_NONE:
      default:
        lineFollowTurnLeft();
        Serial.println("[LINE] Lost line → sweeping LEFT (default)");
        break;
    }
  }
}

// ============================================================
// --- Serial Command Handler ---
// ============================================================

void handleSerial() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  switch (cmd) {
    case 'l':
      lineFollowing = true;
      lastTurn = LAST_TURN_NONE;
      lastLinePoll = 0;
      Serial.println("[LINE] Line follow ON -- send 'k' to stop");
      break;

    case 'k':
      lineFollowing = false;
      stopAll();
      Serial.println("[LINE] Line follow OFF");
      break;

    default:
      Serial.println("[CMD] l=start k=stop");
      break;
  }
}

// ============================================================
// --- Arduino Lifecycle ---
// ============================================================

void setup() {
  Serial.begin(9600);
  Serial.println("=== Drisky Line Follow ===");
  Serial.println("l=start  k=stop");

  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    if (MOTORS[i].en != 255) pinMode(MOTORS[i].en, OUTPUT);
    pinMode(MOTORS[i].in1, OUTPUT);
    pinMode(MOTORS[i].in2, OUTPUT);
    digitalWrite(MOTORS[i].in1, LOW);
    digitalWrite(MOTORS[i].in2, LOW);
    if (MOTORS[i].en != 255) analogWrite(MOTORS[i].en, 0);
  }

  pinMode(IR_LINE_PIN, INPUT);

  Serial.print("[IR] Startup reading: ");
  Serial.println(digitalRead(IR_LINE_PIN) == LOW ? "ON LINE" : "off line");

  Serial.println("Ready.");
}

void loop() {
  handleSerial();
  tickLineFollow();
}