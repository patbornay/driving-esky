/**
 * RC Car - Motor Test + IR Obstacle Sensor Logging
 * Arduino Uno + IRF3205 MOSFETs + Duinotech XC4524 IR Sensors
 *
 * MOTOR PIN ASSIGNMENTS (2 pins per motor: pinA=PWM fwd, pinB=digital rev):
 *   Motor FL : pinA=D3,  pinB=A0
 *   Motor FR : pinA=D5,  pinB=A1
 *   Motor RL : pinA=D6,  pinB=A2
 *   Motor RR : pinA=D9,  pinB=A3
 *
 * IR SENSOR WIRING (Duinotech XC4524):
 *   VCC --> 5V on Arduino
 *   GND --> GND (common)
 *   OUT --> Digital pin (see below)
 *   EN  --> Leave disconnected
 *
 *   Sensor FRONT-LEFT  : OUT --> D10
 *   Sensor FRONT-RIGHT : OUT --> D11
 *
 *   Logic: LOW = obstacle detected, HIGH = clear
 *
 * COMMANDS:
 *   w      - forward at current speed
 *   s      - reverse at current speed
 *   a      - turn left  (left reverse, right forward)
 *   d      - turn right (left forward, right reverse)
 *   x      - stop
 *   0-9    - set speed (1=10% ... 9=90%, 0=100%)
 *   f/r    - ramp test forward/reverse
 *   i      - print current IR sensor readings
 */

#include <Arduino.h>

// ============================================================
// --- Types ---
// ============================================================

typedef enum {
  FORWARD,
  REVERSE,
  BRAKE,
  COAST
} MotorDir;

typedef struct {
  uint8_t pinA;  // forward gate
  uint8_t pinB;  // reverse gate
} Motor;

// Tracks what the car is currently doing (for sensor log context)
typedef enum {
  STATE_STOPPED,
  STATE_FORWARD,
  STATE_REVERSE,
  STATE_TURN_LEFT,
  STATE_TURN_RIGHT
} CarState;

// ============================================================
// --- Motor Definitions ---
// ============================================================

const Motor MOTORS[] = {
  {3, A0},  // FL: D3=PWM fwd, A0=digital rev
  {5, A1},  // FR: D5=PWM fwd, A1=digital rev
  {6, A2},  // RL: D6=PWM fwd, A2=digital rev
  {9, A3},  // RR: D9=PWM fwd, A3=digital rev
};
const uint8_t NUM_MOTORS = 4;

#define MOTOR_FL 0
#define MOTOR_FR 1
#define MOTOR_RL 2
#define MOTOR_RR 3

// ============================================================
// --- IR Sensor Definitions ---
// ============================================================

#define IR_FRONT_LEFT_PIN  10
#define IR_FRONT_RIGHT_PIN 11

// LOW = obstacle detected (active low)
#define IR_OBSTACLE   LOW
#define IR_CLEAR      HIGH

struct IRReadings {
  bool frontLeftBlocked;
  bool frontRightBlocked;
};

// ============================================================
// --- State ---
// ============================================================

uint8_t  currentSpeed = 128;           // default 50%
CarState carState     = STATE_STOPPED;
const uint8_t TURN_SPEED = 51;         // 20% of 255

// How often to poll sensors while moving (ms)
#define SENSOR_POLL_MS 200
unsigned long lastSensorPoll = 0;

// ============================================================
// --- Forward Declarations ---
// ============================================================

void driveMotor(Motor m, MotorDir dir, uint8_t speed);
void driveAll(MotorDir dir, uint8_t speed);
void stopAll();
void brakeAll();
void turnLeft();
void turnRight();
IRReadings readIR();
void logIR(IRReadings r, const char* context);
void pollIRIfMoving();

// ============================================================
// --- Core Motor Control ---
// ============================================================

void driveMotor(Motor m, MotorDir dir, uint8_t speed) {
  switch (dir) {
    case FORWARD:
      analogWrite(m.pinA, speed);
      digitalWrite(m.pinB, LOW);
      break;
    case REVERSE:
      digitalWrite(m.pinA, LOW);
      digitalWrite(m.pinB, HIGH);  // full on, no PWM on analog pins
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
  carState = STATE_STOPPED;
}

void brakeAll() {
  driveAll(BRAKE, 0);
  carState = STATE_STOPPED;
}

void turnLeft() {
  driveMotor(MOTORS[MOTOR_FL], REVERSE, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RL], REVERSE, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_FR], FORWARD, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RR], FORWARD, TURN_SPEED);
  carState = STATE_TURN_LEFT;
}

void turnRight() {
  driveMotor(MOTORS[MOTOR_FL], FORWARD, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RL], FORWARD, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_FR], REVERSE, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RR], REVERSE, TURN_SPEED);
  carState = STATE_TURN_RIGHT;
}

// ============================================================
// --- IR Sensor ---
// ============================================================

IRReadings readIR() {
  IRReadings r;
  r.frontLeftBlocked  = (digitalRead(IR_FRONT_LEFT_PIN)  == IR_OBSTACLE);
  r.frontRightBlocked = (digitalRead(IR_FRONT_RIGHT_PIN) == IR_OBSTACLE);
  return r;
}

void logIR(IRReadings r, const char* context) {
  Serial.print("[IR] ");
  Serial.print(context);
  Serial.print(" | FL:");
  Serial.print(r.frontLeftBlocked  ? "BLOCKED" : "clear  ");
  Serial.print(" FR:");
  Serial.println(r.frontRightBlocked ? "BLOCKED" : "clear");
}

const char* stateLabel() {
  switch (carState) {
    case STATE_FORWARD:    return "FORWARD   ";
    case STATE_REVERSE:    return "REVERSE   ";
    case STATE_TURN_LEFT:  return "TURN_LEFT ";
    case STATE_TURN_RIGHT: return "TURN_RIGHT";
    default:               return "STOPPED   ";
  }
}

// Poll IR sensors periodically while the car is moving
void pollIRIfMoving() {
  if (carState == STATE_STOPPED) return;

  unsigned long now = millis();
  if (now - lastSensorPoll >= SENSOR_POLL_MS) {
    lastSensorPoll = now;
    IRReadings r = readIR();
    logIR(r, stateLabel());
  }
}

// ============================================================
// --- Wheel Test Helpers ---

void testMotor(uint8_t idx, const char* label) {
  Serial.print("[TEST] Running ");
  Serial.print(label);
  Serial.print(" at ");
  Serial.print(currentSpeed);
  Serial.println("/255 -- send x to stop");
  driveMotor(MOTORS[idx], FORWARD, currentSpeed);
}

void testFrontWheels() {
  Serial.print("[TEST] Front wheels at ");
  Serial.print(currentSpeed);
  Serial.println("/255 -- send x to stop");
  driveMotor(MOTORS[MOTOR_FL], FORWARD, currentSpeed);
  driveMotor(MOTORS[MOTOR_FR], FORWARD, currentSpeed);
}

void testRearWheels() {
  Serial.print("[TEST] Rear wheels at ");
  Serial.print(currentSpeed);
  Serial.println("/255 -- send x to stop");
  driveMotor(MOTORS[MOTOR_RL], FORWARD, currentSpeed);
  driveMotor(MOTORS[MOTOR_RR], FORWARD, currentSpeed);
}


// ============================================================

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

// ============================================================
// --- Serial Command Handler ---
// ============================================================

void handleSerial() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  switch (cmd) {
    case 'g':
      stopAll();
      testFrontWheels();
      break;

    case 'h':
      stopAll();
      testRearWheels();
      break;

    case 'v':
      stopAll();
      testMotor(MOTOR_FL, "FL (Front Left)");
      break;

    case 'b':
      stopAll();
      testMotor(MOTOR_FR, "FR (Front Right)");
      break;

    case 'n':
      stopAll();
      testMotor(MOTOR_RL, "RL (Rear Left)");
      break;

    case 'm':
      stopAll();
      testMotor(MOTOR_RR, "RR (Rear Right)");
      break;

    case 'w':
      driveAll(FORWARD, currentSpeed);
      carState = STATE_FORWARD;
      Serial.print("[CMD] Forward at ");
      Serial.print(currentSpeed);
      Serial.println("/255");
      break;

    case 's':
      driveAll(REVERSE, currentSpeed);
      carState = STATE_REVERSE;
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

    case 'i': {
      // Manual IR snapshot
      IRReadings r = readIR();
      logIR(r, stateLabel());
      break;
    }

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
      rampForward();
      break;

    case 'r':
      rampReverse();
      break;

    default:
      Serial.println("[CMD] Unknown. w=fwd s=rev a=left d=right x=stop i=IR 0-9=speed");
      Serial.println("         g=front h=rear v=FL b=FR n=RL m=RR f/r=ramp");
      break;
  }
}

// ============================================================
// --- Arduino Lifecycle ---
// ============================================================

void setup() {
  Serial.begin(9600);
  Serial.println("=== Drisky ===");
  Serial.println("w=fwd s=rev a=left d=right x=stop i=IR 0-9=speed");
  Serial.println("g=front h=rear v=FL b=FR n=RL m=RR");

  // Motor pins
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    pinMode(MOTORS[i].pinA, OUTPUT);
    pinMode(MOTORS[i].pinB, OUTPUT);
    digitalWrite(MOTORS[i].pinA, LOW);
    digitalWrite(MOTORS[i].pinB, LOW);
  }

  // IR sensor pins
  pinMode(IR_FRONT_LEFT_PIN,  INPUT);
  pinMode(IR_FRONT_RIGHT_PIN, INPUT);

  // Initial sensor reading
  IRReadings r = readIR();
  logIR(r, "STARTUP   ");

  Serial.println("Ready.");
}

void loop() {
  handleSerial();
  pollIRIfMoving();
}