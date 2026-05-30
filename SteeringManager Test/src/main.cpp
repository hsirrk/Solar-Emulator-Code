#include <Arduino.h>
#include "SteeringManager.h"  
#include <WiFi.h>
#include "WifiHelper.h" 

    
DCMotor left = DCMotor(22,19,1,2,15); 
DCMotor right = DCMotor(20,21,3,4,15);    

SteeringManager steer(&left,&right);

double kp = 0;
double kd = 0;
int baseSpeed = 800;


void setup() {
  left.begin();
  right.begin();

  steer.begin(35,34,36,39); // put IR pins here -> left to right vp: 36 vn: 39

  Serial.begin(115200);

  WifiHelper wifi = WifiHelper();
  wifi.begin();
  Serial.println("Wifi Server Started");
  wifi.startTune(&kp, &kd, &baseSpeed);
  baseSpeed = constrain(baseSpeed,0,1000);
  steer.setPID(kp,kd);
  delay(2000);
  
}

void loop() {
  
  // Line Following
  steer.lineFollow(baseSpeed);

  // Reversing
  // steer.array.takeReading(true);
  // while(!steer.array.isOnLine()) {
  //   delay(1000);
  //   steer.array.takeReading(true);
  // }
  // Serial.println("Found line, starting reverse");
  // delay(1000);
  // steer.reverse(800);
  // delay(5000);

  // steer.array.takeReading(false);
  // steer.array.getError();
  // steer.array.showState();
  // Serial.print("\n");
  // steer.array.update();

  // Serial.println("begin");
  // left.drivePWM(800);
  // delay(2000);
  // right.drivePWM(800);
  // delay(2000);


}