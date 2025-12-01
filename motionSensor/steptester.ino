#include <Stepper.h>

// Steps per revolution (change this if your motor is different)
const int STEPS_PER_REV = 2048;

// Pin assignments for the stepper driver (ULN2003 or 4-wire driver)
Stepper stepperMotor(STEPS_PER_REV, 4, 5, 6, 7);

// Switch pin
const int SWITCH_PIN = 2;

void setup() {
  pinMode(SWITCH_PIN, INPUT_PULLUP);  
  stepperMotor.setSpeed(10);  // RPM of the motor
}

void loop() {
  // Read switch (LOW = pressed because of INPUT_PULLUP)
  bool pressed = (digitalRead(SWITCH_PIN) == LOW);

  if (pressed) {
    // Rotate forward by small step
    stepperMotor.step(10);
  } 
}