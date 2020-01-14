#include "ESPGW32.h"

#define HOST ""     // server ip
#define PORT ""     // server udp port

CprE_NB_bc95 modem;
char sock[] = "0\0";
unsigned long prev_t = 0;
unsigned long interval = 5000;
unsigned long cntUpNumber = 1;    // data example

void setup() {
  Serial.begin(9600);
  
  // configure serial to connect with NBIoT Shield
  Serial2.begin(9600,SERIAL_8N1,Uno8,Uno9);
  modem.init(Serial2);
  modem.initModem();
  
  Serial.println("BEGIN");
  Serial.println("IMEI = " + modem.getIMEI());
  Serial.println("IMSI = " + modem.getIMSI());
  int8_t cntdown = 10;
  while(!modem.register_network()) {
    --cntdown;
    if(cntdown <= 0) {
      Serial.println("Connect to network fail! - restart ESP!");
      ESP.restart();    // restart ESP when cannot connect to network
    }
  }
  Serial.println("IP   = " + modem.check_ipaddr());
  modem.create_UDP_socket(4700,sock);
  Serial.println("RSSI = " + String(modem.check_modem_signal()));
  Serial.println();
}

void loop() {
  unsigned long curr_t = millis();
  if(curr_t-prev_t > interval || prev_t == 0) {
    prev_t = curr_t;
    String data1 = "\"NBIoT-node\",\"device1\",";
    data1 += (String)cntUpNumber;
    Serial.print("packet send : ");
    Serial.println(data1);
    modem.sendUDPstr(HOST,PORT,data1);
    ++cntUpNumber;
  }
}
