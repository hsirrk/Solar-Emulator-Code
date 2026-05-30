#pragma once

#include <Arduino.h>

class IRArray {
    private:
    
        int pin1, pin2, pin3, pin4; // left to right
        boolean r1,r2,r3,r4; // corresponding pin readings
        int error, lastError;
    public:

        IRArray();

        void begin(int outerLeftPin, int innerLeftPin, int innerRightPin, int outerRightPin);

        void takeReading(boolean show);
        void showReading();
        void showState();
        int getError();
        void update();
        boolean isOnLine();
        boolean isCentered();
        
};