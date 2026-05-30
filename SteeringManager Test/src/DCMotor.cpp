#include "DCMotor.h"

DCMotor::DCMotor(int motorPin1, int motorPin2, int pwmChannel1, int pwmChannel2, int maxVoltage) {
    this->motorPin1 = motorPin1;
    this->motorPin2 = motorPin2;
    this->pwmChannel1 = pwmChannel1;
    this->pwmChannel2 = pwmChannel2;
    this->maxVoltage = maxVoltage;
}

/**
 * Initialize the DCMotor
 * Sets up the PWM channels for the motor pins
 */
void DCMotor::begin() {
    ledcSetup(this->pwmChannel1, this->pwmFrequency, this->pwmResolution);
    ledcAttachPin(motorPin1, pwmChannel1);
    ledcSetup(this->pwmChannel2, this->pwmFrequency, this->pwmResolution);
    ledcAttachPin(motorPin2, pwmChannel2);

    this->maxDutyCycle = map(this->maxVoltage, 0, this->hBridgeVoltage, 0, 4096);
}

/**
 * Drive the motor with a certain duty cycle. 
 * @param signedDuty positive sign means driving IN1 HIGH, negative sign means driving IN2 HIGH.
 * When looking at the side of the motor with the terminals, 
 *  Driving IN1 HIGH will rotate the motor clockwise.
 *  Driving IN2 HIGH will rotate the motor counter-clockwise.
 */
void DCMotor::drivePWM(int signedDuty) {
    int duty = abs(signedDuty);
    duty = constrain(duty, 0, this->maxDutyCycle);

    if (signedDuty > 0) {
        ledcWrite(pwmChannel2, 0);
        ledcWrite(pwmChannel1, duty);
    } else {
        ledcWrite(pwmChannel1, 0);
        ledcWrite(pwmChannel2, duty);
    }
}

/**
 * Stop the motor by setting PWM channels to 0.
 */
void DCMotor::stop() {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);
}

/**
 * Get the maximum duty cycle for the motor
 * @return maximum duty cycle
 */
int DCMotor::getMaxDutyCycle() {
    return this->maxDutyCycle;
}