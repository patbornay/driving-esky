/**
 * Drisky - Line Tracer
 * Arduino Uno + L298N Motor Driver (2 motors, EN jumpers on)
 *
 * WIRING:
 *   Motor A (LEFT wheel)  : IN1=D2, IN2=D3
 *   Motor B (RIGHT wheel) : IN1=D4, IN2=D5
 *   ENA/ENB jumpers ON (full speed)
 *   Common GND between Arduino and L298N
 *
 * IR SENSOR (Duinotech XC4524):
 *   OUT → A0
 *   VCC → 5V, GND → GND, EN → leave disconnected
 *   Logic: LOW = line detected, HIGH = off line
 *
 * COMMANDS:
 *   l - start line tracing
 *   x - stop
 */

#include <Arduino.h>

// ============================================================
// --- Pin Definitions ---
// ============================================================

#define LEFT_IN1  2
#define LEFT_IN2  3
#define RIGHT_IN1 4
#define RIGHT_IN2 5

#define IR_PIN A0

// How long each wheel drives before checking sensor again (ms)
#define TRACE_DELAY_MS 300
// ============================================================

bool tracing = false;

// Which wheel is currently driving
typedef enum {
  DRIVING_RIGHT,  // right wheel forward = turning left
  DRIVING_LEFT    // left wheel forward  = turning right
} ActiveWheel;

ActiveWheel activeWheel = DRIVING_RIGHT;

// ============================================================
// --- Wheel Control ---
// ============================================================

void leftWheelForward() {
  digitalWrite(LEFT_IN1, HIGH);
  digitalWrite(LEFT_IN2, LOW);
}

void leftWheelStop() {
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, LOW);
}

void rightWheelForward() {
  digitalWrite(RIGHT_IN1, HIGH);
  digitalWrite(RIGHT_IN2, LOW);
}

void rightWheelStop() {
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

  if (activeWheel == DRIVING_RIGHT) {
    if (!lineDetected) {
      Serial.println("[TRACE] Line lost → switch to LEFT wheel");
      rightWheelStop();
      leftWheelForward();
      activeWheel = DRIVING_LEFT;
      delay(TRACE_DELAY_MS);
    }
  } else {
    if (lineDetected) {
      Serial.println("[TRACE] Line found → switch to RIGHT wheel");
      leftWheelStop();
      rightWheelForward();
      activeWheel = DRIVING_RIGHT;
      delay(TRACE_DELAY_MS);
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
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);
  pinMode(IR_PIN,    INPUT);

  stopAll();

  Serial.println("=== Drisky Line Tracer ===");
  Serial.print("[IR] Startup: ");
  Serial.println(digitalRead(IR_PIN) == LOW ? "ON LINE" : "off line");
  Serial.println("l=start  x=stop");
  Serial.println("Ready.");
}

void loop() {
  handleSerial();
  tickLineTrace();
}