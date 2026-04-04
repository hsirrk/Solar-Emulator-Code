#include <Arduino.h>
#include <Servo.h>

// Stepper
#define STEPPER_DIR_PIN    8
#define STEPPER_STEP_PIN   4   
#define STEPPER_LIMIT_PIN  13
#define MICROSTEPS         32
#define STEP_ANGLE         0.9

const float STEPS_PER_REV = (360.0 / STEP_ANGLE) * MICROSTEPS;
float stepperCurrAngle = 0.0;
float stepperUpperLimit = 20.0;
float stepperLowerLimit = -20.0;

// Servo
Servo myServo;
#define SERVO_PIN          6   
#define SERVO_BUTTON_PIN   2

const int errorCCW = -10;
const int errorCW = 10;
int servoStoredAngle   = 0;     // deviation from home in degrees
int servoOffset        = 0;     // raw offset found during homing
int SERVO_MAX_DEV         = 30; // max deviation from home (editable)
const int SERVO_STEP_SIZE = 5;  // degrees per F/B command
int servoHomeAngle     = 0;     // absolute servo angle for home
int servoDirection     = 1;     // +1 if home is low (move toward 180), -1 if high (move toward 0)
bool servoHomed        = false;
char servoPrevCmd      = ' ';

void stepperHome();
void stepperMoveToAngle(float angle);
void stepperPulse();
bool servoHomeSequence();
void servoReturnHome();
int clampServoTarget(int angle);
void servoApplyStep(bool forward);
void servoApplyMovement(int deltaDegrees);

void setup() {
  Serial.begin(9600);

  // --- Stepper pin setup ---
  pinMode(STEPPER_STEP_PIN, OUTPUT);
  pinMode(STEPPER_DIR_PIN, OUTPUT);
  pinMode(STEPPER_LIMIT_PIN, INPUT_PULLUP);

  // --- Servo pin setup ---
  myServo.attach(SERVO_PIN);
  pinMode(SERVO_BUTTON_PIN, INPUT_PULLUP);
  myServo.write(20);   // move servo to 20 starting position
  delay(1000);

  // --- Home both motors ---
  Serial.println("STEPPER:Homing...");
  stepperHome();
  Serial.println("STEPPER:Homed");

  Serial.println("SERVO:Homing...");
  servoHomed = servoHomeSequence();
  if (servoHomed) {
    servoHomeAngle = servoOffset;
    myServo.write(servoHomeAngle);
    delay(1000);  // let servo settle at home
    // Direction is always -1: sweep goes 180->0, trigger is above home,
    // so positive deviation moves AWAY from trigger (toward 0).
    servoDirection = -1;
    Serial.print("SERVO:Home angle=");
    Serial.println(servoHomeAngle);
    Serial.println("SERVO:Homed");
  } else {
    Serial.println("SERVO:ERROR:Homing failed");
  }

  Serial.println("READY");
}

int error(int homeAngle) {
  if(myServo.read() < homeAngle){
    return errorCW;
  }
  else{
    return errorCCW;
  }
}

void servoReturnHome() {
  servoStoredAngle = 0;
  // Overshoot above home, then approach from above (same direction as homing sweep)
  myServo.write(clampServoTarget(servoHomeAngle + 2 * SERVO_STEP_SIZE));
  delay(500);
  myServo.write(servoHomeAngle);
  delay(2000);
  servoPrevCmd = 'r';
}

int clampServoTarget(int angle) {
  if (angle < 0) return 0;
  if (angle > 180) return 180;
  return angle;
}

void servoApplyStep(bool forward) {
  int current = myServo.read();

  if (forward) {
    if (servoPrevCmd != 'f') {
      myServo.write(clampServoTarget(current + 2 * SERVO_STEP_SIZE));
      delay(150);
      current = myServo.read();
    }
    myServo.write(clampServoTarget(current + SERVO_STEP_SIZE));
    servoPrevCmd = 'f';
    servoStoredAngle += SERVO_STEP_SIZE;
  } else {
    if (servoPrevCmd == 'f') {
      myServo.write(clampServoTarget(current - (2 * SERVO_STEP_SIZE)));
      delay(150);
      current = myServo.read();
    }
    myServo.write(clampServoTarget(current - SERVO_STEP_SIZE));
    servoPrevCmd = 'b';
    servoStoredAngle -= SERVO_STEP_SIZE;
  }

  delay(300);
}

void servoApplyMovement(int deltaDegrees) {
  if (deltaDegrees == 0) return;

  bool forward = deltaDegrees > 0;
  int remaining = abs(deltaDegrees);

  while (remaining >= SERVO_STEP_SIZE) {
    servoApplyStep(forward);
    remaining -= SERVO_STEP_SIZE;
  }

  if (remaining > 0) {
    int current = myServo.read();
    int signedRemainder = forward ? remaining : -remaining;
    myServo.write(clampServoTarget(current + signedRemainder));
    servoStoredAngle += signedRemainder;
    servoPrevCmd = forward ? 'f' : 'b';
    delay(300);
  }
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() < 2) return;

    char motorType = input.charAt(0);
    String cmd = input.substring(1);
    cmd.trim();

    if (motorType == 'S' || motorType == 's') {
      // Stepper cmd: S<float> 
      float angle = cmd.toFloat();
      stepperMoveToAngle(angle);
    }
    else if (motorType == 'L' || motorType == 'l') {
      // Limit chng: LU<val>  (stepper upper), LL<val> (stepper lower), LS<val> (servo max)
      char limType = cmd.charAt(0);
      float val = cmd.substring(1).toFloat();
      if (limType == 'U' || limType == 'u') {
        stepperUpperLimit = val;
        Serial.print("LIMIT:STEPPER_UPPER:");
        Serial.println(stepperUpperLimit);
      } else if (limType == 'L' || limType == 'l') {
        stepperLowerLimit = val;
        Serial.print("LIMIT:STEPPER_LOWER:");
        Serial.println(stepperLowerLimit);
      } else if (limType == 'S' || limType == 's') {
        SERVO_MAX_DEV = (int)val;
        Serial.print("LIMIT:SERVO_MAX:");
        Serial.println(SERVO_MAX_DEV);
      } else {
        Serial.println("LIMIT:ERROR:Unknown limit type");
      }
    }
    else if (motorType == 'V' || motorType == 'v') {
      // Servo
      if (!servoHomed) {
        Serial.println("SERVO:ERROR:Not homed");
        return;
      }

      if (cmd.equalsIgnoreCase("F")) {
        servoApplyMovement(SERVO_STEP_SIZE);
      }
      else if (cmd.equalsIgnoreCase("B")) {
        servoApplyMovement(-SERVO_STEP_SIZE);
      }
      else if (cmd.equalsIgnoreCase("R")) {
        // Reset to home 
        servoReturnHome();
        Serial.println("SERVO:0");
        return;
      }
      else {
        // Custom angle: V<float>
        int angle = (int)cmd.toFloat();
        servoApplyMovement(angle);
      }

      // Clamp check for all movement commands (F/B/custom)
      if (servoStoredAngle > SERVO_MAX_DEV || servoStoredAngle < -SERVO_MAX_DEV) {
        servoReturnHome();
        Serial.println("SERVO:ERROR:Limit exceeded, returning home");
      } else {
        Serial.print("SERVO:");
        Serial.print(servoStoredAngle);
        Serial.print(" [target=");
        Serial.print(myServo.read());
        Serial.println("]");
      }
    }
  }
}

void stepperHome() {
  digitalWrite(STEPPER_DIR_PIN, HIGH);
  while (true) {
    stepperPulse();
    bool limitRead = digitalRead(STEPPER_LIMIT_PIN);
    delay(1);
    bool prevLimitRead = limitRead;
    if (limitRead == HIGH && prevLimitRead == HIGH) {
      // Rotate 4° to set rig flat, then define as home
      stepperMoveToAngle(4);
      stepperCurrAngle = 0;
      return;
    }
  }
}

void stepperMoveToAngle(float angle) {
  long steps = (long)(abs(angle) * STEPS_PER_REV / 360.0);
  stepperCurrAngle += angle;

  if (angle < 0) {
    digitalWrite(STEPPER_DIR_PIN, LOW);
  } else {
    digitalWrite(STEPPER_DIR_PIN, HIGH);
  }

  for (long i = 0; i < steps; i++) {
    stepperPulse();
  }

  Serial.print("STEPPER:");
  Serial.println(stepperCurrAngle);
}

void stepperPulse() {
  digitalWrite(STEPPER_STEP_PIN, HIGH);
  delayMicroseconds(500);
  digitalWrite(STEPPER_STEP_PIN, LOW);
  delayMicroseconds(500);
}

bool debounceButton() {
  // Require 3 consecutive HIGH reads to confirm trigger
  for (int i = 0; i < 3; i++) {
    delay(5);
    if (digitalRead(SERVO_BUTTON_PIN) != HIGH) return false;
  }
  return true;
}

bool servoHomeSequence() {
  // Move servo to 180° and let it fully settle before sweeping.
  // Without this, the servo is still traveling from 0° and button
  // readings at high angles are inaccurate.
  myServo.write(180);
  delay(2000);

  int coarseAngle = -1;
  for (int angle = 180; angle >= 0; angle--) {
    myServo.write(angle);
    delay(60);
    if (digitalRead(SERVO_BUTTON_PIN) == HIGH && debounceButton()) {
      coarseAngle = angle;
      break;
    }
  }
  if (coarseAngle < 0) return false;

  int backoff = min(coarseAngle + 15, 180);
  myServo.write(backoff);
  delay(700);  // let servo fully settle after coarse pass
  
  for (int angle = backoff; angle >= 0; angle--) {
    myServo.write(angle);
    delay(150);  // slower for precision
    if (digitalRead(SERVO_BUTTON_PIN) == HIGH && debounceButton()) {
      // Set home far enough past the trigger that the ENTIRE
      // +/- SERVO_MAX_DEV range stays on the free side (below trigger).
      // This prevents any commanded position from hitting the button.
      int homeAngle = angle + error(angle);
      myServo.write(homeAngle);
      delay(1000);  // settle at home position
      servoOffset = homeAngle;
      Serial.print("SERVO:Trigger=");
      Serial.println(angle);
      return true;
    }
  }
  return false;
}