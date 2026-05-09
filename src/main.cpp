/**
 * RC Car - Basic Motor Test
 * Arduino Uno + IRF3205 MOSFETs (low-side switch, one per motor)
 *
 * Wiring per motor:
 *   Motor(+) ---> 12V
 *   Motor(-) ---> IRF3205 Drain
 *   IRF3205 Source --> GND (common with Arduino GND)
 *   IRF3205 Gate  --> 100Ω --> PWM Pin
 *                            +-- 10kΩ --> GND  (pull-down, prevents float)
 *
 * PWM Pins used (all support analogWrite on Uno):
 *   Motor FL (Front Left)  : D3
 *   Motor FR (Front Right) : D5
 *   Motor RL (Rear Left)   : D6
 *   Motor RR (Rear Right)  : D9
 */

#include <Arduino.h>

// --- Types (must come first) ---
typedef enum {
  FORWARD,
  REVERSE,
  BRAKE,
  COAST
} MotorDir;

typedef struct {
  uint8_t pinA;
  uint8_t pinB;
} Motor;

// --- Forward Declarations ---
void driveAll(MotorDir dir, uint8_t speed);
void stopAll();
void brakeAll();

// --- Pin Definitions ---
const uint8_t PIN_MOTOR_FL = 3;   // Front Left
const uint8_t PIN_MOTOR_FR = 5;   // Front Right
const uint8_t PIN_MOTOR_RL = 6;   // Rear Left
const uint8_t PIN_MOTOR_RR = 9;   // Rear Right

const uint8_t MOTOR_PINS[] = {PIN_MOTOR_FL, PIN_MOTOR_FR, PIN_MOTOR_RL, PIN_MOTOR_RR};
const uint8_t NUM_MOTORS = 4;

// --- Motor Control ---

/**
 * Set speed of a single motor.
 * @param pin   PWM pin connected to MOSFET gate
 * @param speed 0 (off) to 255 (full speed)
 */
void setMotor(uint8_t pin, uint8_t speed) {
  analogWrite(pin, speed);
}

/**
 * Set all four motors to the same speed.
 */
void setAllMotors(uint8_t speed) {
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    setMotor(MOTOR_PINS[i], speed);
  }
}

/**
 * Stop all motors immediately.
 */
void stopAll() {
  setAllMotors(0);
}

// --- Test Routines ---

/**
 * Ramp all motors from 0 → max → 0 smoothly.
 * Good first test to confirm wiring before adding direction control.
 */
void rampTest() {
  Serial.println("[TEST] Ramping UP...");
  for (int spd = 0; spd <= 5000; spd += 1000) {
    setAllMotors(spd);
    delay(30);
  }

  Serial.println("[TEST] Holding full speed for 10s...");
  delay(10000);

  Serial.println("[TEST] Ramping DOWN...");
  for (int spd = 5000; spd >= 0; spd -= 1000) {
    setAllMotors(spd);
    delay(30);
  }

  stopAll();
  Serial.println("[TEST] Done. Motors stopped.");
}

/**
 * Simple serial command interface for manual testing.
 * Commands:
 *   'r'        - run ramp test
 *   '0'–'9'   - set speed to 0%, 11%, 22%... 100%
 *   's'        - stop
 */
void handleSerial() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  if (cmd == 'r') {
    rampTest();
  } else if (cmd == 's') {
    stopAll();
    Serial.println("[CMD] Stopped.");
  } else if (cmd >= '0' && cmd <= '9') {
    case '0' ... '9': {
      // '1'=10%, '2'=20% ... '9'=90%, '0'=100% (full send)
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
  } else {
    Serial.println("[CMD] Unknown. Use: r=ramp, s=stop, 0-9=speed");
  }
}

// --- Arduino Lifecycle ---

void setup() {
  Serial.begin(9600);
  Serial.println("=== RC Car Motor Test ===");
  Serial.println("Commands: r=ramp test, s=stop, 0-9=set speed");

  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    pinMode(MOTOR_PINS[i], OUTPUT);
    digitalWrite(MOTOR_PINS[i], LOW);  // Ensure MOSFETs start OFF
  }

  Serial.println("Motors initialised. Ready.");
}

void loop() {
  handleSerial();
}