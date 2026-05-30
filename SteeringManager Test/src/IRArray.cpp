#include "IRArray.h"
#include "GlobalConstants.h"

/**
 * IRArray object, consists of 4 IR sensors
 * <0 (negative) is left, >0 (positive) is right
 * 0 is low, 1 is high
 */

IRArray::IRArray() {
    error = 0;
    lastError = 0;
}

/**
 * Initialize the IRArray with pin numbers
 * @param outerLeftPin pin number for the outer left IR sensor
 * @param innerLeftPin pin number for the inner left IR sensor
 * @param innerRightPin pin number for the inner right IR sensor
 * @param outerRightPin pin number for the outer right IR sensor
 */
void IRArray::begin(int outerLeftPin, int innerLeftPin, int innerRightPin, int outerRightPin) {
    pin1 = outerLeftPin;
    pin2 = innerLeftPin;
    pin3 = innerRightPin;
    pin4 = outerRightPin;

    pinMode(pin1, INPUT);
    pinMode(pin2, INPUT);
    pinMode(pin3, INPUT);
    pinMode(pin4, INPUT);
}

/**
 * Take a reading from the IR sensors
 * @param show if true, prints the reading to Serial
 */
void IRArray::takeReading(boolean show) {
    r1 = !digitalRead(pin1);
    r2 = !digitalRead(pin2);
    r3 = !digitalRead(pin3);
    r4 = !digitalRead(pin4);
    if (show) {
        showReading();
    }
}

/**
 * Print the current reading of the IR sensors to Serial
 * Format: r1 r2 r3 r4
 */
void IRArray::showReading() {
    Serial.printf("%d %d %d %d\n", r1, r2, r3, r4);
}

/**
 * Print the current state of the IRArray
 * Format: r1 r2 r3 r4 -- error lastError
 */
void IRArray::showState() {
    Serial.printf("%d %d %d %d -- %d %d", r1, r2, r3, r4, error, lastError); // newline was deleted for printing with PID values
}

/**
 * Get the current error value based on the IR sensor readings
 * @return error value
 */
int IRArray::getError() {

    if (r2&&r3) error = 0; // on the line

    else if (r1||r2) { // on left side
        if (!r1 && r2) { // inner left
            error = -ERR_1;            
        }
        else if (r1 && r2) { // mid left
            error = -ERR_2;
        }
        else if (r1 && !r2) { // outer left
            error = -ERR_3;
        }
    }

    else if (r3||r4) { // on right side
        if (!r4 && r3) { // inner right
            error = ERR_1;            
        }
        else if (r4 && r3) { // mid right
            error = ERR_2;
        }
        else if (r4 && !r3) { // outer right
            error = ERR_3;
        }
    }

    else if (!r1&&!r2&&!r3&&!r4) { // totally off line
        if (lastError < 0) { // on the left
            error = -ERR_4;
        }

        else {
            error = ERR_4;
        }   
    }

    return error;
}

/**
 * Update the last error value to the current error value
 * This is used to keep track of the previous state of the IRArray
 */
void IRArray::update() {
    lastError = error;
    return;
}

/**
 * Check if the robot is on the line
 * @return true if on line, false otherwise
 */
boolean IRArray::isOnLine() {
    return (r1 || r2 || r3 || r4); // if any of the readings are high then the robot should be on the line;
}

/**
 * Check if the robot is centered on the line
 * @return true if centered, false otherwise
 */
boolean IRArray::isCentered() {
    return (r2 && r3); // if both inner sensors are high then the robot is centered on the line
}