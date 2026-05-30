#include "SteeringManager.h"
#include "GlobalConstants.h"

volatile boolean drive = false; // boolean indicating when to stop driving ; should be global and changed via interrupts

SteeringManager::SteeringManager(DCMotor* left, DCMotor* right)
    : leftMotor(left), rightMotor(right),
      pidController(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT)
{

}

/**
 * Initialize the DCMotor
 * Sets up PID, motors, and IR sensors
 * @param outerLeftPin Pin for the outer left IR sensor
 * @param innerLeftPin Pin for the inner left IR sensor 
 * @param innerRightPin Pin for the inner right IR sensor
 * @param outerRightPin Pin for the outer right IR sensor
 */
void SteeringManager::begin(int outerLeftPin, int innerLeftPin, int innerRightPin, int outerRightPin) {
    // PID
    this->pidController.SetMode(AUTOMATIC);
    this->pidController.SetOutputLimits(-leftMotor->getMaxDutyCycle(), leftMotor->getMaxDutyCycle()); // Set output limits to motor max duty cycle
    this->pidController.SetSampleTime(this->PIDSampleTime);

    // Motor
    leftMotor->begin();
    rightMotor->begin();

    // IR
    this->array.begin(outerLeftPin, innerLeftPin, innerRightPin, outerRightPin);
}

/**
 * Drive both motors forward with the same duty cycle
 * @param duty the positive duty cycle to drive the motors forwards with
 */
void SteeringManager::forward(int duty) {
    drive = true;
    while (drive) {
        leftMotor->drivePWM(duty);
        rightMotor->drivePWM(duty);
    }
    this->stop();
}

/**
 * Drive both motors backward with the same duty cycle
 * @param duty the positive duty cycle to drive the motors backwards with
 */
void SteeringManager::backward(int duty) {
    drive = true;
    while (drive) {
        leftMotor->drivePWM(-duty);
        rightMotor->drivePWM(-duty);
    }
    this->stop();
}

/**
 * Stop both motors
 * Sets the PWM channels to 0
 */
void SteeringManager::stop() {
    leftMotor->stop();
    rightMotor->stop();
}

/**
 * Reverse the motors in place until the IR sensors detect that they are not on the line
 * This is used to turn in place
 * @param duty the positive duty cycle to drive the motors with
 */
void SteeringManager::turnAround(int duty, boolean clockwise) {

    // IMPORTANT:
    // for some reason [left -> negative, right -> positive] is clockwise

    // clockwise means the robot is moving right so error should be positive
    // counter-clockwise means the robot is moving left so error should be negative
    
    if (!clockwise) {
        duty = -duty; // if counter-clockwise, invert the duty cycle
    }

    this->array.takeReading(false);
    if (!this->array.isOnLine()) {
        return; //do nothing if not on line
    }

    while (this->array.isOnLine()) {
        // turn in place until off line
        this->array.takeReading(true);
        this->array.getError();
        this->array.update();
        leftMotor->drivePWM(-duty);
        rightMotor->drivePWM(duty);
    }

    Serial.println("Off line now");
    delay(1000);

    while (!this->array.isCentered()) {
        // turn in place until back on line
        leftMotor->drivePWM(-duty);
        rightMotor->drivePWM(duty);
        this->array.takeReading(true);
        this->array.getError();
        this->array.update();
    }
    Serial.println("Finish reverse");

    // while (!this->array.isCentered()) {
    //     // turn in place until back on line
    //     leftMotor->drivePWM(duty/2);
    //     rightMotor->drivePWM(-duty/2);
    // }
    stop();
}

/**
 * Line follow using the IR sensors and PID controller
 * @param baseSpeed the base speed to drive the motors at
 * The PID controller will adjust the speed of the motors based on the error from the IR sensors
 */
void SteeringManager::lineFollow(int baseSpeed) {
    // Serial.println()
    drive = true;
    this->array.takeReading(false);
    input = this->array.getError();
    unsigned long lastComputeTime = millis();
    this->array.update();
    while (drive) {
        // only poll and calculate PID at PID sample rate
        if (millis() - lastComputeTime >= this->PIDSampleTime) {
            // compute PID
            pidController.Compute();
            lastComputeTime = millis();
            // drive motors
            leftMotor->drivePWM(baseSpeed-output);
            rightMotor->drivePWM(baseSpeed+output);
            
        }
        // update IR data every cycle so that error is accurate
        this->array.takeReading(false);
        input = this->array.getError();
        array.showState();
        Serial.printf(" -- %lf\n", output);
        this->array.update();
    }
    this->stop();
}

/**
 * Set the PID controller tunings
 * @param kp Proportional gain
 * @param kd Derivative gain
 */
void SteeringManager::setPID(double kp, double kd) {
    pidController.SetTunings(kp,kd,0);
}