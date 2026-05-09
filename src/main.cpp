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
 * COMMANDS:
 *   w      - forward (all motors)
 *   s      - reverse (all motors)
 *   a      - turn left  (left wheels reverse, right wheels forward)
 *   d      - turn right (left wheels forward, right wheels reverse)
 *   x      - stop
 *   0-9    - set speed (1=10%, 2=20% ... 9=90%, 0=100%)
 *   f/r    - ramp test forward/reverse
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
//                FL      FR      RL      RR
const Motor MOTORS[] = {
  {3, 2},   // FL (left side)
  {5, 4},   // FR (right side)
  {6, 7},   // RL (left side)
  {9, 8}    // RR (right side)
};
const uint8_t NUM_MOTORS = 4;

// Motor index aliases for readability
#define MOTOR_FL 0
#define MOTOR_FR 1
#define MOTOR_RL 2
#define MOTOR_RR 3

// Current speed (0-255), adjusted by 0-9 keys
uint8_t currentSpeed = 128;  // default 50%

// Turn speed is 20% of max (255)
const uint8_t TURN_SPEED = 51;  // 255 * 0.20 = 51

// --- Forward Declarations ---
void driveMotor(Motor m, MotorDir dir, uint8_t speed);
void driveAll(MotorDir dir, uint8_t speed);
void stopAll();
void brakeAll();
void turnLeft();
void turnRight();

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

// Left wheels reverse, right wheels forward
void turnLeft() {
  driveMotor(MOTORS[MOTOR_FL], REVERSE, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RL], REVERSE, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_FR], FORWARD, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RR], FORWARD, TURN_SPEED);
}

// Left wheels forward, right wheels reverse
void turnRight() {
  driveMotor(MOTORS[MOTOR_FL], FORWARD, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RL], FORWARD, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_FR], REVERSE, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RR], REVERSE, TURN_SPEED);
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

// --- Serial Command Handler ---

void handleSerial() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  switch (cmd) {
    case 'w':
      driveAll(FORWARD, currentSpeed);
      Serial.print("[CMD] Forward at ");
      Serial.print(currentSpeed);
      Serial.println("/255");
      break;

    case 's':
      driveAll(REVERSE, currentSpeed);
      Serial.print("[CMD] Reverse at ");
      Serial.print(currentSpeed);
      Serial.println("/255");
      break;

    case 'a':
      turnLeft();
      Serial.println("[CMD] Turning left");
      break;

    case 'd':
      turnRight();
      Serial.println("[CMD] Turning right");
      break;

    case 'x':
      stopAll();
      Serial.println("[CMD] Stopped.");
      break;

    case '0' ... '9': {
      uint8_t percent = (cmd == '0') ? 100 : (cmd - '0') * 10;
      currentSpeed = (uint8_t)(percent * 255 / 100);
      Serial.print("[CMD] Speed set to ");
      Serial.print(percent);
      Serial.print("% (");
      Serial.print(currentSpeed);
      Serial.println("/255) -- send w/s/a/d to move");
      break;
    }

    case 'f':
      rampTest(FORWARD, "FORWARD");
      break;

    case 'r':
      rampTest(REVERSE, "REVERSE");
      break;

    default:
      Serial.println("[CMD] Unknown. w=fwd s=rev a=left d=right x=stop 0-9=speed f/r=ramp");
      break;
  }
}

// --- Arduino Lifecycle ---

void setup() {
  Serial.begin(9600);
  Serial.println("=== Drisky ===");
  Serial.println("w=fwd s=rev a=left d=right x=stop 0-9=set speed");
  Serial.print("Default speed: 50% (128/255)");

  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    pinMode(MOTORS[i].pinA, OUTPUT);
    pinMode(MOTORS[i].pinB, OUTPUT);
    digitalWrite(MOTORS[i].pinA, LOW);
    digitalWrite(MOTORS[i].pinB, LOW);
  }

  Serial.println("\nReady.");
}

void loop() {
  handleSerial();
}