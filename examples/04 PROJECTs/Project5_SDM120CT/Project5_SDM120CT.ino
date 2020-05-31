/***********************************************************************
 * Project       : SDM120CT-MV
 * Board Select  : ESP32vn IoT Uno
 * Description   : Send <EnergyUsage> packet to server every 15 minutes
 *                 and restart itself everyday at 3 pm
 * Packet format : <projectName>,<nodeName>,<data1>,...,<dataN>,<packet_no>
 * Last update   : 31 MAY 2020
 * Author        : CprE13-KMUTNB
 ***********************************************************************
 * Note : 
 * - Beware! Connect ESPGW32 with NBIoT-shield correctly.
 * - Move both JUMPERs to RS485 position.
 * - Wiring Guide: http://www.etteam.com/productDIN/SDM120CT-MV/%E0%B8%81%E0%B8%B2%E0%B8%A3%E0%B8%95%E0%B9%88%E0%B8%AD%E0%B9%83%E0%B8%8A%E0%B9%89%E0%B8%87%E0%B8%B2%E0%B8%99_SMD120CT-MV.pdf
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
  Serial1.begin(2400,SERIAL_8N1,RXmax,TXmax);   // connect to RS485 device
  Serial2.begin(9600,SERIAL_8N1,Uno8,Uno9);     // connect to NBIoT shield
  m_rtu.initSerial(Serial1, DIRPIN);
  modem.init(Serial2);
  Serial.println("# START");
  Serial.println("# IoT PROJECT   : Energy Usage");
  Serial.println("# Modbus Device : SDM120CT-MV");
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

    // modbus query input register
    m_rtu.sendReadInput(SLAVE_ADDR,0,2);      // read voltage
    float volt = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Voltage                : " + (String)volt + " Volts");
    m_rtu.sendReadInput(SLAVE_ADDR,6,2);        // read current
    float current = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Current                : " + (String)current + " Amps");
    m_rtu.sendReadInput(SLAVE_ADDR,12,2);       // read active power
    float actPower = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Active Power           : " + (String)actPower + " Watts");
    m_rtu.sendReadInput(SLAVE_ADDR,18,2);       // read apparent power
    float appPower = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Apparent Power         : " + (String)appPower + " VoltAmps");
    m_rtu.sendReadInput(SLAVE_ADDR,24,2);       // read reactive power
    float reactPower = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Reactive Power         : " + (String)reactPower + " VAr");
    m_rtu.sendReadInput(SLAVE_ADDR,30,2);       // read power factor
    float pfactor = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Power Factor           : " + (String)pfactor);
    m_rtu.sendReadInput(SLAVE_ADDR,36,2);       // read phase angle
    float phase = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Phase Angle            : " + (String)phase + " Degrees");
    m_rtu.sendReadInput(SLAVE_ADDR,70,2);       // read frequency
    float freq = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Frequency              : " + (String)freq + " Hz");
    m_rtu.sendReadInput(SLAVE_ADDR,72,2);       // read import active energy
    float imAct = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Import Active Energy   : " + (String)imAct + " kWh");
    m_rtu.sendReadInput(SLAVE_ADDR,74,2);       // read export active energy
    float exAct = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Export Active Energy   : " + (String)exAct + " kWh");
    m_rtu.sendReadInput(SLAVE_ADDR,76,2);       // read import reactive energy
    float imReact = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Import Reactive Energy : " + (String)imReact + " kVArh");
    m_rtu.sendReadInput(SLAVE_ADDR,78,2);       // read export reactive energy
    float exReact = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Export Reactive Energy : " + (String)exReact + " kVArh");
    m_rtu.sendReadInput(SLAVE_ADDR,342,2);      // read total active energy
    float totalAct = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Total Active Energy    : " + (String)totalAct + " kWh");
    m_rtu.sendReadInput(SLAVE_ADDR,344,2);      // read total reactive energy
    float totalReact = m_rtu.recv_float(SLAVE_ADDR);
    Serial.println("Total Reactive Energy  : " + (String)totalReact + " kVArh");
    
    // send data via NB-IoT
    String packet = "\"SDM120CT-MV\",\"device1\",";
    
    packet += (String)volt;            // voltage
    packet += ",";
    
    packet += (String)current;         // current
    packet += ",";
    
    packet += (String)actPower;        // active power
    packet += ",";
    
    packet += (String)appPower;        // apparent power
    packet += ",";
    
    packet += (String)reactPower;      // reactive power
    packet += ",";
    
    packet += (String)pfactor;         // power factor
    packet += ",";
    
    packet += (String)phase;           // phase angle
    packet += ",";
    
    packet += (String)freq;            // frequency
    packet += ",";
    
    packet += (String)imAct;           // import active energy
    packet += ",";
    
    packet += (String)exAct;           // export active energy
    packet += ",";
    
    packet += (String)imReact;         // import reactive energy
    packet += ",";
    
    packet += (String)exReact;         // export reactive energy
    packet += ",";
    
    packet += (String)totalAct;        // total active energy
    packet += ",";
    
    packet += (String)totalReact;      // total reactive energy
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
