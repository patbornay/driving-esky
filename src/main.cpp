/**
 * Drisky - RC Car
 * Arduino Uno + 2x L298N Motor Driver + 2x Duinotech IR Sensors
 *
 * L298N WIRING:
 *   L298N #1 (Front) - 12V from LiPo #1
 *     ENA → D3  (PWM, FL speed)
 *     IN1 → D2  (FL direction)
 *     IN2 → D4  (FL direction)
 *     ENB → D5  (PWM, FR speed)
 *     IN3 → D7  (FR direction)
 *     IN4 → D8  (FR direction)
 *
 *   L298N #2 (Rear) - 12V from LiPo #2
 *     ENA → D6  (PWM, RL speed)
 *     IN1 → D12 (RL direction)
 *     IN2 → D13 (RL direction)
 *     ENB → D9  (PWM, RR speed)
 *     IN3 → D10 (RR direction)
 *     IN4 → D11 (RR direction)
 *
 *   COMMON GND: Arduino GND + both L298N GNDs + both battery negatives
 *   POWER: L298N #1 5V out → Arduino 5V pin (remove USB once confirmed working)
 *
 * IR SENSOR WIRING (Duinotech XC4524):
 *   Line sensor (single) OUT → A0   (dark line on light surface)
 *   Obstacle sensor       OUT → A1
 *   VCC → 5V, GND → GND, EN → leave disconnected
 *   Logic: LOW = line/obstacle detected, HIGH = clear
 *
 * LINE FOLLOWING LOGIC (1 sensor):
 *   Sensor sees line (LOW)  → go straight
 *   Sensor loses line (HIGH) → keep last turn until line found again
 *
 * COMMANDS:
 *   w      - forward
 *   s      - reverse
 *   a      - turn left  (left reverse, right forward)
 *   d      - turn right (left forward, right reverse)
 *   x      - stop
 *   0-9    - set speed (1=10% ... 9=90%, 0=100%)
 *   g      - front wheels only
 *   h      - rear wheels only
 *   v      - FL only
 *   b      - FR only
 *   n      - RL only
 *   m      - RR only
 *   i      - IR sensor snapshot
 *   l      - start line follow mode
 *   k      - stop line follow mode
 *   f/r    - ramp test forward/reverse
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
  uint8_t en;   // PWM speed (ENA or ENB)
  uint8_t in1;  // direction pin 1
  uint8_t in2;  // direction pin 2
} Motor;

typedef enum {
  STATE_STOPPED,
  STATE_FORWARD,
  STATE_REVERSE,
  STATE_TURN_LEFT,
  STATE_TURN_RIGHT,
  STATE_LINE_FOLLOW
} CarState;

// Last turn direction used during line following (for lost-line recovery)
typedef enum {
  LAST_TURN_NONE,
  LAST_TURN_LEFT,
  LAST_TURN_RIGHT
} LastTurn;

// ============================================================
// --- Motor Definitions ---
// ============================================================

//               EN   IN1  IN2
const Motor MOTORS[] = {
  {20, 2, 3},   // Motor A: no EN pin (jumper on), IN1=D2, IN2=D3
  {20, 4, 5},   // Motor B: no EN pin (jumper on), IN1=D4, IN2=D5
  {20, 2, 3},   // mirrored A (temp)
  {20, 4, 5},   // mirrored B (temp)
};
const uint8_t NUM_MOTORS = 4;

#define MOTOR_FL 0
#define MOTOR_FR 1
#define MOTOR_RL 2
#define MOTOR_RR 3

// ============================================================
// --- IR Sensor Definitions ---
// ============================================================

#define IR_LINE_PIN     A0   // Line following sensor (single)
#define IR_OBSTACLE_PIN A1   // Obstacle detection sensor

#define IR_OBSTACLE LOW   // Active low
#define IR_CLEAR    HIGH

struct IRReadings {
  bool lineDetected;      // true = sensor sees the dark line
  bool obstacleDetected;  // true = obstacle in front
};

// ============================================================
// --- State ---
// ============================================================

uint8_t  currentSpeed  = 20;           // default 50%
CarState carState      = STATE_STOPPED;
LastTurn lastTurn      = LAST_TURN_NONE;

const uint8_t TURN_SPEED      = 20;    // 20% of 255 for tank turns
const uint8_t LINE_SPEED      = 20;   // line follow forward speed (~40%)
const uint8_t LINE_TURN_SPEED = 20;    // line follow turn speed (~31%)

#define SENSOR_POLL_MS    200   // obstacle log interval while moving (ms)
#define LINE_POLL_MS      50    // line follow sensor poll interval (ms)

unsigned long lastSensorPoll = 0;
unsigned long lastLinePoll   = 0;

// ============================================================
// --- Forward Declarations ---
// ============================================================

void driveMotor(Motor m, MotorDir dir, uint8_t speed);
void driveAll(MotorDir dir, uint8_t speed);
void stopAll();
void brakeAll();
void turnLeft();
void turnRight();
void testMotor(uint8_t idx, const char* label);
void testFrontWheels();
void testRearWheels();
IRReadings readIR();
void logIR(IRReadings r, const char* context);
void pollIRIfMoving();
void tickLineFollow();
const char* stateLabel();

// ============================================================
// --- Core Motor Control ---
// ============================================================

/**
 * L298N direction truth table:
 *   IN1=HIGH IN2=LOW  → forward
 *   IN1=LOW  IN2=HIGH → reverse
 *   IN1=LOW  IN2=LOW  → coast
 *   IN1=HIGH IN2=HIGH → brake
 *   EN=0               → always off
 */
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
    case BRAKE:
      digitalWrite(m.in1, HIGH);
      digitalWrite(m.in2, HIGH);
      if (m.en != 255) analogWrite(m.en, 255);
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
  carState = STATE_STOPPED;
}

void brakeAll() {
  driveAll(BRAKE, 0);
  carState = STATE_STOPPED;
}

// Left wheels reverse, right wheels forward
void turnLeft() {
  driveMotor(MOTORS[MOTOR_FL], REVERSE, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RL], REVERSE, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_FR], FORWARD, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RR], FORWARD, TURN_SPEED);
  carState = STATE_TURN_LEFT;
}

// Left wheels forward, right wheels reverse
void turnRight() {
  driveMotor(MOTORS[MOTOR_FL], FORWARD, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RL], FORWARD, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_FR], REVERSE, TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RR], REVERSE, TURN_SPEED);
  carState = STATE_TURN_RIGHT;
}

// Line follow variants — gentler speed, no state change (managed by tickLineFollow)
void lineFollowStraight() {
  driveAll(FORWARD, LINE_SPEED);
}

void lineFollowTurnLeft() {
  driveMotor(MOTORS[MOTOR_FL], REVERSE, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RL], REVERSE, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_FR], FORWARD, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RR], FORWARD, LINE_TURN_SPEED);
  lastTurn = LAST_TURN_LEFT;
}

void lineFollowTurnRight() {
  driveMotor(MOTORS[MOTOR_FL], FORWARD, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RL], FORWARD, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_FR], REVERSE, LINE_TURN_SPEED);
  driveMotor(MOTORS[MOTOR_RR], REVERSE, LINE_TURN_SPEED);
  lastTurn = LAST_TURN_RIGHT;
}

// ============================================================
// --- Wheel Test Helpers ---
// ============================================================

void testMotor(uint8_t idx, const char* label) {
  stopAll();
  Serial.print("[TEST] ");
  Serial.print(label);
  Serial.print(" at ");
  Serial.print(currentSpeed);
  Serial.println("/255 -- send x to stop");
  driveMotor(MOTORS[idx], FORWARD, currentSpeed);
}

void testFrontWheels() {
  stopAll();
  Serial.print("[TEST] Front wheels at ");
  Serial.print(currentSpeed);
  Serial.println("/255 -- send x to stop");
  driveMotor(MOTORS[MOTOR_FL], FORWARD, currentSpeed);
  driveMotor(MOTORS[MOTOR_FR], FORWARD, currentSpeed);
}

void testRearWheels() {
  stopAll();
  Serial.print("[TEST] Rear wheels at ");
  Serial.print(currentSpeed);
  Serial.println("/255 -- send x to stop");
  driveMotor(MOTORS[MOTOR_RL], FORWARD, currentSpeed);
  driveMotor(MOTORS[MOTOR_RR], FORWARD, currentSpeed);
}

// ============================================================
// --- Ramp Tests ---
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

// ============================================================
// --- IR Sensors ---
// ============================================================

IRReadings readIR() {
  IRReadings r;
  // Dark line on light surface: sensor LOW = line detected
  r.lineDetected     = (digitalRead(IR_LINE_PIN)     == IR_OBSTACLE);
  r.obstacleDetected = (digitalRead(IR_OBSTACLE_PIN) == IR_OBSTACLE);
  return r;
}

void logIR(IRReadings r, const char* context) {
  Serial.print("[IR] ");
  Serial.print(context);
  Serial.print(" | LINE:");
  Serial.print(r.lineDetected     ? "ON LINE  " : "off line ");
  Serial.print(" OBSTACLE:");
  Serial.println(r.obstacleDetected ? "BLOCKED" : "clear");
}

const char* stateLabel() {
  switch (carState) {
    case STATE_FORWARD:     return "FORWARD    ";
    case STATE_REVERSE:     return "REVERSE    ";
    case STATE_TURN_LEFT:   return "TURN_LEFT  ";
    case STATE_TURN_RIGHT:  return "TURN_RIGHT ";
    case STATE_LINE_FOLLOW: return "LINE_FOLLOW";
    default:                return "STOPPED    ";
  }
}

// Periodic obstacle log while driving (non-line-follow modes)
void pollIRIfMoving() {
  if (carState == STATE_STOPPED || carState == STATE_LINE_FOLLOW) return;
  unsigned long now = millis();
  if (now - lastSensorPoll >= SENSOR_POLL_MS) {
    lastSensorPoll = now;
    logIR(readIR(), stateLabel());
  }
}

// ============================================================
// --- Line Follow ---
// ============================================================

/**
 * Single-sensor line follow logic:
 *
 *   Sensor ON line  (LOW)  → drive straight
 *   Sensor OFF line (HIGH) → repeat last turn to search for line
 *                            if no last turn yet, turn left by default
 *
 * The sensor should be mounted centre-front of the car.
 * Place it over the line to start — the car will steer to keep
 * the sensor on the line by turning away when it drifts off.
 *
 * With one sensor the car will oscillate slightly — this is normal.
 * Adding a second sensor (A1) later allows much smoother tracking.
 */
void tickLineFollow() {
  if (carState != STATE_LINE_FOLLOW) return;

  unsigned long now = millis();
  if (now - lastLinePoll < LINE_POLL_MS) return;
  lastLinePoll = now;

  IRReadings r = readIR();

  if (r.lineDetected) {
    // On the line — steer opposite of last sweep to re-centre
    switch (lastTurn) {
      case LAST_TURN_LEFT:
        lineFollowTurnRight();
        Serial.println("[LINE] On line → steering RIGHT to centre");
        break;
      case LAST_TURN_RIGHT:
        lineFollowTurnLeft();
        Serial.println("[LINE] On line → steering LEFT to centre");
        break;
      case LAST_TURN_NONE:
      default:
        lineFollowStraight();
        Serial.println("[LINE] On line → straight");
        break;
    }
  } else {
    // Lost the line — sweep opposite of last direction to find it again
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
      lastTurn = LAST_TURN_NONE;
      Serial.println("[CMD] Stopped.");
      break;

    case 'l':
      carState = STATE_LINE_FOLLOW;
      lastTurn = LAST_TURN_LEFT;
      lastLinePoll = 0;
      Serial.println("[LINE] Line follow mode ON -- send 'k' to stop");
      Serial.print("  LINE_SPEED=");
      Serial.print(LINE_SPEED);
      Serial.print("  TURN_SPEED=");
      Serial.println(LINE_TURN_SPEED);
      break;

    case 'k':
      stopAll();
      lastTurn = LAST_TURN_NONE;
      Serial.println("[LINE] Line follow mode OFF");
      break;

    case 'g':
      testFrontWheels();
      break;

    case 'h':
      testRearWheels();
      break;

    case 'v':
      testMotor(MOTOR_FL, "FL (Front Left)");
      break;

    case 'b':
      testMotor(MOTOR_FR, "FR (Front Right)");
      break;

    case 'n':
      testMotor(MOTOR_RL, "RL (Rear Left)");
      break;

    case 'm':
      testMotor(MOTOR_RR, "RR (Rear Right)");
      break;

    case 'i': {
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
      Serial.println("/255)");
      break;
    }

    case 'f':
      rampTest(FORWARD, "FORWARD");
      break;

    case 'r':
      rampTest(REVERSE, "REVERSE");
      break;

    default:
      Serial.println("[CMD] Unknown.");
      Serial.println("  w=fwd s=rev a=left d=right x=stop");
      Serial.println("  l=line follow ON  k=line follow OFF");
      Serial.println("  i=IR snapshot  0-9=speed  f/r=ramp");
      Serial.println("  g=front h=rear v=FL b=FR n=RL m=RR");
      break;
  }
}

// ============================================================
// --- Arduino Lifecycle ---
// ============================================================

void setup() {
  Serial.begin(9600);
  Serial.println("=== Drisky ===");
  Serial.println("w=fwd s=rev a=left d=right x=stop");
  Serial.println("l=line follow ON  k=line follow OFF");
  Serial.println("i=IR  0-9=speed  g=front h=rear v=FL b=FR n=RL m=RR");

  // Motor pins
for (uint8_t i = 0; i < NUM_MOTORS; i++) {
  if (MOTORS[i].en != 255) pinMode(MOTORS[i].en, OUTPUT);
  pinMode(MOTORS[i].in1, OUTPUT);
  pinMode(MOTORS[i].in2, OUTPUT);
  digitalWrite(MOTORS[i].in1, LOW);
  digitalWrite(MOTORS[i].in2, LOW);
  if (MOTORS[i].en != 255) analogWrite(MOTORS[i].en, 0);
}

  // IR sensor pins
  pinMode(IR_LINE_PIN,     INPUT);
  pinMode(IR_OBSTACLE_PIN, INPUT);

  // Startup IR reading
  logIR(readIR(), "STARTUP    ");

  Serial.print("Default speed: 50% (128/255)\n");
  Serial.println("Ready.");
}

void loop() {
  handleSerial();
  tickLineFollow();
  pollIRIfMoving();
}