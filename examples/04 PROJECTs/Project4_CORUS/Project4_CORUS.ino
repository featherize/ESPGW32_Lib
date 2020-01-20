/***********************************************************************
 * Project       : CORUS - Gas Volume Converter
 * Board Select  : ESP32vn IoT Uno
 * Description   : Send <GasSupply> packet to server every 15 minutes
 *                 and restart itself everyday at 3 pm
 * Packet format : <projectName>,<nodeName>,<unconvIndex>,<convIndex>,
 *                 <instantT>,<instantP>,<battCounter>,<packet_no>
 * Last update   : 20 JAN 2020
 * Author        : CprE13-KMUTNB
 ***********************************************************************
 * Note : 
 * - Beware! Connect ESPGW32 with NBIoT-shield correctly.
 * - Move both JUMPERs to RS232 position.
***********************************************************************/

#include "ESPGW32.h"

#define HOST        ""      // server ip
#define PORT        ""      // server udp port
#define SLAVE_ADDR  1       // Modbus Slave Address

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
  Serial.println("# IoT PROJECT   : Gas Supply");
  Serial.println("# Modbus Device : CORUS - Gas Volume Converter");
  Serial.println("# Interface     : RS232");

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
    m_rtu.sendReadHolding(SLAVE_ADDR,806,2);      // read unconvIndex (MVRI)
    long MVRI = m_rtu.recv_int(SLAVE_ADDR);
    Serial.println("Unconverted index = " + (String)MVRI);

    m_rtu.sendReadHolding(SLAVE_ADDR,810,2);      // read convIndex (MVRI)
    long MVBI = m_rtu.recv_int(SLAVE_ADDR);
    Serial.println("Converted index = " + (String)MVBI);

    m_rtu.sendReadHolding(SLAVE_ADDR,834,2);      // read instantT (MTG)
    float MTG = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Instant. T = " + (String)MTG);

    m_rtu.sendReadHolding(SLAVE_ADDR,836,2);      // read instantP (MPG)
    float MPG = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Instant. P = " + (String)MPG);
    
    m_rtu.sendReadHolding(SLAVE_ADDR,548,1);      // read battCounter (XSBR)
    int XSBR = m_rtu.recv_int(SLAVE_ADDR);
    Serial.println("Battery autonomy counter = " + (String)XSBR);

    // send data via NB-IoT
    String packet = "\"CORUS\",\"device1\","; 
    packet += (String)MVRI;
    packet += ",";
    packet += (String)MVBI;
    packet += ",";
    packet += (String)MTG;
    packet += ",";
    packet += (String)MPG;
    packet += ",";
    packet += (String)XSBR;
    packet += ",";
    packet += (String)(packet_no++);
    if(cntInt == 0) {
      Serial.print("packet send : ");
      Serial.println(packet);
      Serial.println("<---------------------------------------->");
      modem.sendUDPstr(HOST,PORT,packet);
    }
  }
  
  // When ESP32 is interrupted by RTC
  if(cntInt > 0) {
    ESP.restart();    // restart ESP when got an interrupt
  }
}
