#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// -------- I2C config --------
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define SLAVE_ADDR 0x08    // address of outside ESP

// -------- LCD config --------
LiquidCrystal_I2C lcd(0x27, 16, 2);  // change 0x27 if your LCD uses another address

// -------- Stepper config (28BYJ-48 + ULN2003) --------
#define IN1 4
#define IN2 5
#define IN3 6
#define IN4 7

// 8-step half-stepping sequence
const int stepSequence[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

int currentStepIndex = 0;

// motion states
uint8_t motionState = 0;      // current motion from outside node
uint8_t prevMotionState = 0;  // previous value for edge detection

// -------- Functions --------

void stepMotorOneStep(int direction) {
  // direction: +1 = forward, -1 = backward
  currentStepIndex += direction;
  if (currentStepIndex > 7) currentStepIndex = 0;
  if (currentStepIndex < 0) currentStepIndex = 7;

  digitalWrite(IN1, stepSequence[currentStepIndex][0]);
  digitalWrite(IN2, stepSequence[currentStepIndex][1]);
  digitalWrite(IN3, stepSequence[currentStepIndex][2]);
  digitalWrite(IN4, stepSequence[currentStepIndex][3]);
}

void rotateSteps(int steps, int direction, int stepDelayMs) {
  for (int i = 0; i < steps; i++) {
    stepMotorOneStep(direction);
    delay(stepDelayMs);
  }
}

void runStepperOnMotion() {
  // Only trigger when motion goes from 0 -> 1
  if (prevMotionState == 0 && motionState == 1) {
    Serial.println("Motion detected -> running stepper");

    // Example: rotate forward 512 steps
    // 512 half-steps ~ one revolution for many 28BYJ-48 motors
    rotateSteps(512, +1, 3);  // steps, direction, delay per step ms

    // Optionally rotate back:
    // rotateSteps(512, -1, 3);
  }
}

void updateLCD() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("SMART DOOR SYS");

  lcd.setCursor(0, 1);
  lcd.print("MOTION: ");
  if (motionState) {
    lcd.print("DETECTED");
  } else {
    lcd.print("NONE     ");
  }
}

void setup() {
  Serial.begin(115200);

  // I2C master
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // LCD init
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SMART DOOR SYS");
  lcd.setCursor(0, 1);
  lcd.print("STARTING...");

  // Stepper pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Make sure coils are off initially
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  Serial.println("Inside node (I2C master, LCD, stepper) ready");
}

void loop() {
  prevMotionState = motionState;

  // Request 1 byte from outside ESP
  Wire.requestFrom(SLAVE_ADDR, (uint8_t)1);

  if (Wire.available()) {
    motionState = Wire.read();
    Serial.print("Motion from slave: ");
    Serial.println(motionState ? "DETECTED" : "NONE");
    updateLCD();
  } else {
    Serial.println("No data from slave");
  }

  // Check for new motion and run stepper if needed
  runStepperOnMotion();

  delay(200);  // polling rate
}