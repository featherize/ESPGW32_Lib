/***********************************************************************
 * Project       : YGC-FX
 * Board Select  : ESP32vn IoT Uno
 * Description   : Send <Wind direction> packet to server every 15 minutes
 *                 and restart itself everyday at 3 pm
 * Packet format : <projectName>,<nodeName>,<data>,<packet_no>
 * Last update   : 14 JAN 2020
 * Author        : CprE13-KMUTNB
 ***********************************************************************
 * Note : 
 * - Beware! Connect ESPGW32 with NBIoT-shield correctly.
 * - Move both JUMPERs to RS485 position.
 * - Wiring with YGC-FX (RED:12V),(BLACK:A+),(YELLOW:B-),(GREEN:GND)
 * - Install YGC-FX correctly for correct data
***********************************************************************/

#include "ESPGW32.h"

#define HOST        ""      // server ip
#define PORT        ""      // server udp port
#define SLAVE_ADDR  5       // Modbus Slave Address

CprE_DS3231 rtc(SDA,SCL);
CprE_modbusRTU m_rtu;
CprE_NB_bc95 modem;

char sock[] = "0\0";
int cntInt = 0;
unsigned long prev_t = 0;
unsigned long interval = 900000;   // interval time of each packet

void IRAM_ATTR isr() {
    Serial.println("*** INTERRUPT! ***");
    ++cntInt;   // increase <cntInt> when got interrupted
}

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600,SERIAL_8N1,RXmax,TXmax);   // connect to RS485 device
  Serial2.begin(9600,SERIAL_8N1,Uno8,Uno9);     // connect to NBIoT shield
  m_rtu.initSerial(Serial1, DIRPIN);
  modem.init(Serial2);
  Serial.println("# START");
  Serial.println("# IoT PROJECT   : Wind direction");
  Serial.println("# Modbus Device : YGC-FX");
  Serial.println("# Interface     : RS485");

  // init NBIoT shield
  Serial.println("# Initial NBIoT Board");
  modem.initModem();
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

  // init RTC
  Serial.println("# Initial RTC");
  rtc.clearFlag();
  rtc.disableAlarm();
  if(rtc.lostPower()) {
    Serial.println("Power lost! Set RTC time!");
    rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));
  }
  rtc.setAlarm1(15,00,00,0,false,'D');  // interrupt every day at 3pm
  rtc.enableAlarm(1);
  Serial.println("Time   = " + rtc.currentTime());
  Serial.println("Alarm1 = " + rtc.getAlarm1());

  // attach interrupt pin
  Serial.println("# Attach interrupt pin to RTC's <INT> pin");
  pinMode(RTCINT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTCINT), isr, FALLING);
  Serial.println("**************************************************");
}

void loop() {
  unsigned long curr_t = millis();
  static unsigned long packet_no = 1;
  
  if(curr_t-prev_t > interval || prev_t == 0) {
    prev_t = curr_t;

    // modbus query holding register
    m_rtu.sendReadHolding(SLAVE_ADDR,0,1);      // read wind direction
    int dir = m_rtu.recv_int(SLAVE_ADDR);
    Serial.println("Wind direction = " + (String)dir + "\u00B0 (clockwise from south)");

    // send data via NB-IoT
    String packet = "\"YGC-FX\",\"device1\","; 
    packet += (String)dir;
    packet += ",";
    packet += (String)(packet_no++);
    Serial.print("packet send : ");
    Serial.println(packet);
    Serial.println("<---------------------------------------->");
    modem.sendUDPstr(HOST,PORT,packet);
  }
  
  // When ESP32 is interrupted by RTC
  if(cntInt > 0) {
    ESP.restart();    // restart ESP when got an interrupt
  }
}
