/**
 * Drisky - Line Tracer (with PWM speed control)
 * Arduino Uno + L298N Motor Driver (2 motors)
 *
 * WIRING:
 *   Motor A (LEFT wheel)  : IN1=D2, IN2=D3, ENA=D9  (PWM)
 *   Motor B (RIGHT wheel) : IN1=D4, IN2=D5, ENB=D10 (PWM)
 *   *** REMOVE the ENA and ENB jumpers — wire ENA→D9, ENB→D10 ***
 *   Common GND between Arduino and L298N
 *
 * IR SENSOR (Duinotech XC4524):
 *   OUT → A0
 *   VCC → 5V, GND → GND, EN → leave disconnected
 *   Logic: LOW = line detected, HIGH = off line
 *
 * COMMANDS (Serial @ 9600 baud):
 *   l - start line tracing
 *   x - stop
 */

#include <Arduino.h>

// ============================================================
// --- Pin Definitions ---
// ============================================================

#define LEFT_IN1  2
#define LEFT_IN2  3
#define LEFT_EN   9   // PWM pin — ENA jumper MUST be removed

#define RIGHT_IN1 4
#define RIGHT_IN2 5
#define RIGHT_EN  10  // PWM pin — ENB jumper MUST be removed

#define IR_PIN    A0

// How long each wheel drives before checking sensor again (ms)
#define TRACE_DELAY_MS 300

// Default motor speed (0–255). 80 ≈ 31% duty — nice and slow.
#define DEFAULT_SPEED  160

// ============================================================

bool tracing = false;

typedef enum {
  DRIVING_RIGHT,  // right wheel forward = robot curves left
  DRIVING_LEFT    // left wheel forward  = robot curves right
} ActiveWheel;

ActiveWheel activeWheel = DRIVING_RIGHT;

// ============================================================
// --- Wheel Control ---
// ============================================================

void leftWheelForward() {
  analogWrite(LEFT_EN, DEFAULT_SPEED);
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, HIGH);
}

void rightWheelForward() {
  analogWrite(RIGHT_EN, DEFAULT_SPEED);
  digitalWrite(RIGHT_IN1, HIGH);
  digitalWrite(RIGHT_IN2, LOW);
}

void leftWheelStop() {
  analogWrite(LEFT_EN, 0);
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, LOW);
}

void rightWheelStop() {
  analogWrite(RIGHT_EN, 0);
  digitalWrite(RIGHT_IN1, LOW);
  digitalWrite(RIGHT_IN2, LOW);
}

void stopAll() {
  leftWheelStop();
  rightWheelStop();
}

// ============================================================
// --- Line Tracer ---
// ============================================================

void tickLineTrace() {
  if (!tracing) return;

  bool lineDetected = (digitalRead(IR_PIN) == LOW);

  if (lineDetected) {
      Serial.println("[TRACE] Line lost → switch to LEFT wheel");
      rightWheelStop();
      leftWheelForward();
      activeWheel = DRIVING_LEFT;
      delay(TRACE_DELAY_MS);
  } else {
      Serial.println("[TRACE] Line found → switch to RIGHT wheel");
      leftWheelStop();
      rightWheelForward();
      activeWheel = DRIVING_RIGHT;
      delay(TRACE_DELAY_MS);
  }

  // if (activeWheel == DRIVING_RIGHT) {
  //   Serial.print("Active wheel driving right \n");
  //   if (!lineDetected) {
  //     Serial.println("[TRACE] Line lost → switch to LEFT wheel");
  //     rightWheelStop();
  //     leftWheelForward();
  //     activeWheel = DRIVING_LEFT;
  //     delay(TRACE_DELAY_MS);
  //   }
  // } else {
  //   Serial.print("Active wheel driving left \n");
  //   if (lineDetected) {
  //     Serial.println("[TRACE] Line found → switch to RIGHT wheel");
  //     leftWheelStop();
  //     rightWheelForward();
  //     activeWheel = DRIVING_RIGHT;
  //     delay(TRACE_DELAY_MS);
  //   }
  // }
}

// ============================================================
// --- Serial Command Handler ---
// ============================================================

void handleSerial() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  switch (cmd) {
    case 'r':
      rightWheelForward();
      break;
    case 't':
      leftWheelForward();
      break;
    case 'l':
      tracing     = true;
      activeWheel = DRIVING_RIGHT;
      rightWheelForward();
      leftWheelStop();
      Serial.println("[TRACE] Started — RIGHT wheel driving, turning left");
      break;
    case 'x':
      tracing = false;
      stopAll();
      Serial.println("[TRACE] Stopped");
      break;

    default:
      Serial.println("[CMD] l=start  x=stop");
      break;
  }
}

// ============================================================
// --- Arduino Lifecycle ---
// ============================================================

void setup() {
  Serial.begin(9600);

  pinMode(LEFT_IN1,  OUTPUT);
  pinMode(LEFT_IN2,  OUTPUT);
  pinMode(LEFT_EN,   OUTPUT);

  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);
  pinMode(RIGHT_EN,  OUTPUT);

  pinMode(IR_PIN, INPUT);

  stopAll();

  Serial.println("=== Drisky Line Tracer (PWM) ===");
  Serial.print("[SPEED] ");
  Serial.print(DEFAULT_SPEED);
  Serial.print("/255 (");
  Serial.print((DEFAULT_SPEED * 100) / 255);
  Serial.println("%)");
  Serial.print("[IR] Startup: ");
  Serial.println(digitalRead(IR_PIN) == LOW ? "ON LINE" : "off line");
  Serial.println("l=start  x=stop");
  Serial.println("Ready.");
}

void loop() {
  handleSerial();
  tickLineTrace();
}