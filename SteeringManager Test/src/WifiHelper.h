#pragma once

#include <Arduino.h>
#include <WiFi.h>

class WifiHelper {
    public:

        const char* AP_ssid;
        const char* AP_pass;
        WiFiServer server;
        bool collected;

        WifiHelper();

        void begin();
        void startTune(double* kp, double* kd, int* baseSpeed);   
};