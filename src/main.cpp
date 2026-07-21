#include <Arduino.h>

// Dual-channel H-bridge driver (IRF3205-based module), PWM+DIR interface
// Channel 1 = left side motors, Channel 2 = right side motors

const int PWM2 = 6;   // Right channel speed
const int DIR2 = 7;   // Right channel direction
const int PWM1 = 5;   // Left channel speed
const int DIR1 = 4;   // Left channel direction

const int DRIVE_SPEED = 200; // 0-255, tune as needed

// speed: -255 to 255. Positive = forward, negative = reverse, 0 = stop
void setLeftMotor(int speed) {
  speed = constrain(speed, -255, 255); // ensures speed is at most 255 and at lowest -255
  // digitalWrite sets a digital pin to either HIGH (5V on the Uno) or LOW(0V/ground)
  // just for controlled direction in this case
  digitalWrite(DIR1, speed >= 0 ? HIGH : LOW);
  // fakes an 'in-between' voltage using PWM (rapidly switching on/off to simulate a lower avg voltage e.g. speed control)
  analogWrite(PWM1, abs(speed));
}

void setRightMotor(int speed) {
  speed = constrain(speed, -255, 255);
  digitalWrite(DIR2, speed >= 0 ? HIGH : LOW);
  analogWrite(PWM2, abs(speed));
}

void driveForward(int speed) {
  setLeftMotor(speed);
  setRightMotor(speed);
}

// by going into the negative we flip the signal from HIGH to LOW putting 
// the H bridge into reverse
void driveBackward(int speed) {
  setLeftMotor(-speed);
  setRightMotor(-speed);
}

void turnLeft(int speed) {
  setLeftMotor(-speed);   // left side reverse
  setRightMotor(speed);   // right side forward -> spin left
}

void turnRight(int speed) {
  setLeftMotor(speed);    // left side forward
  setRightMotor(-speed);  // right side reverse -> spin right
}

void stopCar() {
  setLeftMotor(0);
  setRightMotor(0);
}

void setup() {
  pinMode(PWM1, OUTPUT);
  pinMode(DIR1, OUTPUT);
  pinMode(PWM2, OUTPUT);
  pinMode(DIR2, OUTPUT);
  stopCar(); // always start safe
}

void loop() {
  driveForward(DRIVE_SPEED);
  delay(2000);

  stopCar();
  delay(500);

  driveBackward(DRIVE_SPEED);
  delay(2000);

  stopCar();
  delay(500);

  turnLeft(DRIVE_SPEED);
  delay(1000);

  stopCar();
  delay(500);

  turnRight(DRIVE_SPEED);
  delay(1000);

  stopCar();
  delay(2000);
}