#include "ESPGW32.h"

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600,SERIAL_8N1,Uno8,Uno9);
}

void loop() {
  if(Serial2.available() > 0) {
    Serial.write(Serial2.read());
  }
  if(Serial.available() > 0) {
    Serial2.write(Serial.read());
  }
}
