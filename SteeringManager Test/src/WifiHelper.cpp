#include "WifiHelper.h"
#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_bt.h"

WifiHelper::WifiHelper() {
    AP_ssid = "ESP32_Group9";
    AP_pass = "roborobo";
    server = WiFiServer(80);
    boolean collected = false;
}

/**
 * Begin the WiFi helper by starting the soft AP
 * Sets up the AP SSID and password
 */
void WifiHelper::begin() {
    // Start the soft AP
    while (AP_ssid == nullptr || AP_pass == nullptr) {
        Serial.println("AP SSID or Password is not set. Please set them before starting the AP.");
        delay(1000);
    }
    WiFi.softAP(AP_ssid, AP_pass);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("AP started. Connect to %s:%s, IP=%s\n", AP_ssid, AP_pass, ip.toString().c_str());
    server.begin();
}

/**
 * Start the tuning process by waiting for client connections
 * and reading the PID parameters from the client
 * @param kp Pointer to the proportional gain variable
 * @param kd Pointer to the derivative gain variable
 * @param baseSpeed Pointer to the base speed variable
 */
void WifiHelper::startTune(double* kp, double* kd, int* baseSpeed) {
    // WiFi.softAP(AP_ssid, AP_pass);
    // IPAddress ip = WiFi.softAPIP();
    // Serial.printf("AP started. Connect to %s:%s, IP=%s\n", AP_ssid, AP_pass, ip.toString().c_str());
    // server.begin();
    WiFiClient client;
    boolean collected = false;

    while (!collected) {
        // try to form connection with client
        client = server.available();
        while (client && !collected) {
            Serial.println("Client found");
            while (client.connected() && !collected) {
                //Serial.println("Waiting for client to be available");
                if (client.available()) {
                    Serial.printf("Client connected\n");
                    String line = client.readStringUntil('\n');
                    Serial.printf("Received: %s\n", line.c_str());


                    if (line.startsWith("GET ")) {
                        client.stop();            // drop the OS probe
                        break;                    // go back to server.available()
                    }

                    client.println(line); // echo back to matlab
                    Serial.println("Sent!");

                    // Extract Values
                    switch (line.charAt(0)) {
                        case 'P':
                            *kp = (double) line.substring(1).toInt();
                            break;
                        case 'D':
                            *kd = (double) line.substring(1).toInt();
                            break;
                        case 'B':
                            *baseSpeed = line.substring(1).toInt();
                            break;
                        case 's':
                            collected = true;
                            break;
                    }   
                }
            }
        }
        delay(200);
    }
    Serial.printf("kp: %lf - kd: %lf\n", *kp, *kd);
    client.stop();
    Serial.println("Client disconnected");
    // server.stopAll();
    // Serial.println("Server stopped");

    // WiFi.softAPdisconnect(true);  // Disconnect AP
    // WiFi.mode(WIFI_OFF);          // Turn off WiFi mode

    // esp_wifi_stop();              // Stop WiFi task
    // esp_wifi_deinit();            // Deinit WiFi driver
    
    delay(1000);
}

