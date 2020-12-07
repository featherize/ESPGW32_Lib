/***********************************************************************
 * Project       : Solar Cell Plant
 * Board Select  : ESP32vn IoT Uno
 * Description   : Send Solar Cell Plant infomation to server
 *                 via WiFi every 5 minutes (Also save log in SDcard)
 *                 and restart itself everyday at 6 am
 * Packet format : <projectName>,<date>,<time>,
 *                 [<AC1_?>,...],[<AC2_?>,...],[<AC3_?>,...],
 *                 [<AC4_?>,...],[<AC5_?>,...],[<DC1_?>,...],
 *                 [<DC2_?>,...],[<DC3_?>,...],[<DC4_?>,...]
 * Last update   : 07 DEC 2020
 * Author        : CprE13-KMUTNB
 ***********************************************************************
 * Note : 
 * - Move both JUMPERs to RS485 position.
***********************************************************************/

#include "ESPGW32.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SSID        ""    // WIFI name
#define PASS        ""    // WIFI password

#define HOST        ""    // server ip
#define PORT        ""    // server udp port

#define PLC_ADDR     1

CprE_DS3231 rtc(SDA,SCL);
CprE_modbusRTU m_rtu;
WiFiUDP udp;
NTPClient timeClient(udp, 25200);

int cntInt = 0;                     // interrupt flag
unsigned long prev_t = 0;
unsigned long interval = 300000;    // interval time of each packet

int sck = 21;
int miso = 19;
int mosi = 18;
int cs = 14;
const char *logFile = "/solardatalogs.txt";
bool ntpUpdated = false;
unsigned long rtcTime_st;    // RTC time when setup() is running

/******************** SDcard Functions ********************/

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
//    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

/******************** RTC Interrupt Functions ********************/

void IRAM_ATTR isr() {
  Serial.println("*** INTERRUPT! ***");
  ++cntInt;   // increase <cntInt> when got interrupted
}

/******************** Main Application ********************/

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600,SERIAL_8N1,RXmax,TXmax);   // connect to RS485 device
  m_rtu.initSerial(Serial1, DIRPIN);
  delay(1000);
  
  Serial.println(F("# START"));
  Serial.println(F("# IoT PROJECT   : Solar Cell Plant"));
  Serial.println(F("# Modbus Device : PLC SIEMENS S7-1200"));
  Serial.println(F("# Interface     : RS485"));
  Serial.println(F("# Database      : Cloud DB and SDcard"));

  // init WIFI
  Serial.println(F("# Initialize WiFi..."));
  bool wifi_en = true;
  WiFi.begin(SSID, PASS);
  uint8_t waitwifi = 20;
  while (WiFi.status() != WL_CONNECTED) { // wait for connection
    delay(500);
    Serial.print(".");
    waitwifi--;
    if(waitwifi <= 0) {
      Serial.print("Connect Fail!");
      wifi_en = false;
      break;
    }
  }
  if(wifi_en) {
    Serial.println();
    Serial.print(F("Connected to "));
    Serial.println(SSID);
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
  }
  udp.begin(1234);  // open udp at port 1234

  // init RTC
  Serial.println(F("# Initialize RTC..."));
  rtc.clearFlag();
  rtc.disableAlarm();
  timeClient.begin();
  if(timeClient.update()) {
    Serial.println(F("Update time from NTP-Server to RTC"));
    rtc.adjust(DateTime(timeClient.getEpochTime()));
    ntpUpdated = true;
  }
  delay(1);
  rtc.setAlarm1(6,00,00,0,false,'D');  // interrupt every day at 6am
  delay(1);
  rtc.enableAlarm(1);
  delay(1);
  rtcTime_st = rtc.now().unixtime();
  Serial.println("Time   = " + rtc.currentTime());
  Serial.println("Alarm1 = " + rtc.getAlarm1());

  // attach interrupt pin
  Serial.println(F("# Attach interrupt pin to RTC's <INT> pin"));
  pinMode(RTCINT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTCINT), isr, FALLING);

  // Initialize SD card Module
  bool sd_en = true;
  Serial.print(F("# Initialize SD card..."));
  SPI.begin(sck, miso, mosi, cs);
  if(!SD.begin(cs)) {
    Serial.println(F("Card Mount Failed"));
    sd_en = false;
  }
  uint8_t cardType = SD.cardType();
  if((cardType == CARD_NONE) && sd_en) {
    Serial.println(F("No SD card attached"));
    sd_en = false;
  }
  if(sd_en) {
    Serial.println(F("DONE"));
    File file = SD.open(logFile);
    if(!file) {
      Serial.println(F("LogFile doesn't exist"));
      Serial.println(F("Creating file..."));
      // write header of file
      const char *header = "projectName, date, time,"
      
                           "ACpm1_VoltA, ACpm1_VoltB, ACpm1_VoltC,"
                           "ACpm1_AmpA, ACpm1_AmpB, ACpm1_AmpC,"
                           "ACpm1_Wtotal, ACpm1_kWh"
                           
                           "ACpm2_VoltA, ACpm2_VoltB, ACpm2_VoltC,"
                           "ACpm2_AmpA, ACpm2_AmpB, ACpm2_AmpC,"
                           "ACpm2_Wtotal, ACpm2_kWh"
                           
                           "ACpm3_VoltA, ACpm3_VoltB, ACpm3_VoltC,"
                           "ACpm3_AmpA, ACpm3_AmpB, ACpm3_AmpC,"
                           "ACpm3_Wtotal, ACpm3_kWh"
                           
                           "ACpm4_VoltA, ACpm4_VoltB, ACpm4_VoltC,"
                           "ACpm4_AmpA, ACpm4_AmpB, ACpm4_AmpC,"
                           "ACpm4_Wtotal, ACpm4_kWh"
                           
                           "ACpm5_VoltA, ACpm5_VoltB, ACpm5_VoltC,"
                           "ACpm5_AmpA, ACpm5_AmpB, ACpm5_AmpC,"
                           "ACpm5_Wtotal, ACpm5_kWh"
                           
                           "DCpm1_Volt, DCpm1_Amp, DCpm1_Watt"
                           "DCpm1_kWh, DCpm1_AmpHour"
                           
                           "DCpm2_Volt, DCpm2_Amp, DCpm2_Watt"
                           "DCpm2_kWh, DCpm2_AmpHour"
                           
                           "DCpm3_Volt, DCpm3_Amp, DCpm3_Watt"
                           "DCpm3_kWh, DCpm3_AmpHour"
                           
                           "DCpm4_Volt, DCpm4_Amp, DCpm4_Watt"
                           "DCpm4_kWh, DCpm4_AmpHour"
                           
                           "\r\n";
                           
      writeFile(SD, logFile, header);
      delete header;
    }
    else {
      Serial.println(F("File already exists"));
    }
    file.close();
  }
  if(wifi_en || sd_en) {
    Serial.println(F("# Cannot connect to both WiFi and SDCard"));
    Serial.println(F("# Restart ESP"));
    Serial.println(F("**************************************************"));
    ESP.restart();
  }
  Serial.println(F("**************************************************"));
  Serial.println();
  Serial.println();
}

void loop() {
  unsigned long curr_t = millis();
  
  if(curr_t-prev_t > interval || prev_t == 0) {
    prev_t = curr_t;

    // if lost connection, then reconnect
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("Connection lost!");
      WiFi.reconnect();
      Serial.print("Reconnecting to ");
      Serial.println(SSID);
      delay(5000);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }

    /***** Query data from PLC *****/

    // AC_power_meter_1
    
    Serial.println(F("∙ 50kW No1"));
    
    m_rtu.sendReadHolding(PLC_ADDR,100,2);
    float ACpm1_VoltA = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_1 Volt A = "));
    Serial.println(ACpm1_VoltA);
    
    m_rtu.sendReadHolding(PLC_ADDR,102,2);
    float ACpm1_VoltB = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_1 Volt B = "));
    Serial.println(ACpm1_VoltB);
    
    m_rtu.sendReadHolding(PLC_ADDR,104,2);
    float ACpm1_VoltC = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_1 Volt C = "));
    Serial.println(ACpm1_VoltC);
    
    m_rtu.sendReadHolding(PLC_ADDR,106,2);
    float ACpm1_AmpA = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_1 Amp A = "));
    Serial.println(ACpm1_AmpA);
    
    m_rtu.sendReadHolding(PLC_ADDR,108,2);
    float ACpm1_AmpB = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_1 Amp B = "));
    Serial.println(ACpm1_AmpB);
    
    m_rtu.sendReadHolding(PLC_ADDR,110,2);
    float ACpm1_AmpC = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_1 Amp C = "));
    Serial.println(ACpm1_AmpC);
    
    m_rtu.sendReadHolding(PLC_ADDR,112,2);
    float ACpm1_Wtotal = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_1 Watt total = "));
    Serial.println(ACpm1_Wtotal);
    
    m_rtu.sendReadHolding(PLC_ADDR,114,2);
    float ACpm1_kWh = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_1 kWatt Hour = "));
    Serial.println(ACpm1_kWh);
    
    Serial.println(F("=================================================="));
    
    // AC_power_meter_2
    
    Serial.println(F("∙ 50kW No2"));

    m_rtu.sendReadHolding(PLC_ADDR,200,2);
    float ACpm2_VoltA = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_2 Volt A = "));
    Serial.println(ACpm2_VoltA);
    
    m_rtu.sendReadHolding(PLC_ADDR,202,2);
    float ACpm2_VoltB = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_2 Volt B = "));
    Serial.println(ACpm2_VoltB);
    
    m_rtu.sendReadHolding(PLC_ADDR,204,2);
    float ACpm2_VoltC = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_2 Volt C = "));
    Serial.println(ACpm2_VoltC);
    
    m_rtu.sendReadHolding(PLC_ADDR,206,2);
    float ACpm2_AmpA = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_2 Amp A = "));
    Serial.println(ACpm2_AmpA);
    
    m_rtu.sendReadHolding(PLC_ADDR,208,2);
    float ACpm2_AmpB = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_2 Amp B = "));
    Serial.println(ACpm2_AmpB);
    
    m_rtu.sendReadHolding(PLC_ADDR,210,2);
    float ACpm2_AmpC = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_2 Amp C = "));
    Serial.println(ACpm2_AmpC);
    
    m_rtu.sendReadHolding(PLC_ADDR,212,2);
    float ACpm2_Wtotal = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_2 Watt total = "));
    Serial.println(ACpm2_Wtotal);
    
    m_rtu.sendReadHolding(PLC_ADDR,214,2);
    float ACpm2_kWh = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_2 kWatt Hour = "));
    Serial.println(ACpm2_kWh);
    
    Serial.println(F("=================================================="));
        
    // AC_power_meter_3 
    
    Serial.println(F("∙ 50kW No3"));

    m_rtu.sendReadHolding(PLC_ADDR,300,2);
    float ACpm3_VoltA = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_3 Volt A = "));
    Serial.println(ACpm3_VoltA);
    
    m_rtu.sendReadHolding(PLC_ADDR,302,2);
    float ACpm3_VoltB = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_3 Volt B = "));
    Serial.println(ACpm3_VoltB);
    
    m_rtu.sendReadHolding(PLC_ADDR,304,2);
    float ACpm3_VoltC = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_3 Volt C = "));
    Serial.println(ACpm3_VoltC);
    
    m_rtu.sendReadHolding(PLC_ADDR,306,2);
    float ACpm3_AmpA = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_3 Amp A = "));
    Serial.println(ACpm3_AmpA);
    
    m_rtu.sendReadHolding(PLC_ADDR,308,2);
    float ACpm3_AmpB = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_3 Amp B = "));
    Serial.println(ACpm3_AmpB);
    
    m_rtu.sendReadHolding(PLC_ADDR,310,2);
    float ACpm3_AmpC = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_3 Amp C = "));
    Serial.println(ACpm3_AmpC);
    
    m_rtu.sendReadHolding(PLC_ADDR,312,2);
    float ACpm3_Wtotal = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_3 Watt total = "));
    Serial.println(ACpm3_Wtotal);
    
    m_rtu.sendReadHolding(PLC_ADDR,314,2);
    float ACpm3_kWh = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_3 kWatt Hour = "));
    Serial.println(ACpm3_kWh);
    
    Serial.println(F("=================================================="));
    
    // AC_power_meter_4
    
    Serial.println(F("∙ 125kW No1"));

    m_rtu.sendReadHolding(PLC_ADDR,400,2);
    float ACpm4_VoltA = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_4 Volt A = "));
    Serial.println(ACpm4_VoltA);
    
    m_rtu.sendReadHolding(PLC_ADDR,402,2);
    float ACpm4_VoltB = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_4 Volt B = "));
    Serial.println(ACpm4_VoltB);
    
    m_rtu.sendReadHolding(PLC_ADDR,404,2);
    float ACpm4_VoltC = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_4 Volt C = "));
    Serial.println(ACpm4_VoltC);
    
    m_rtu.sendReadHolding(PLC_ADDR,406,2);
    float ACpm4_AmpA = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_4 Amp A = "));
    Serial.println(ACpm4_AmpA);
    
    m_rtu.sendReadHolding(PLC_ADDR,408,2);
    float ACpm4_AmpB = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_4 Amp B = "));
    Serial.println(ACpm4_AmpB);
    
    m_rtu.sendReadHolding(PLC_ADDR,410,2);
    float ACpm4_AmpC = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_4 Amp C = "));
    Serial.println(ACpm4_AmpC);
    
    m_rtu.sendReadHolding(PLC_ADDR,412,2);
    float ACpm4_Wtotal = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_4 Watt total = "));
    Serial.println(ACpm4_Wtotal);
    
    m_rtu.sendReadHolding(PLC_ADDR,414,2);
    float ACpm4_kWh = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_4 kWatt Hour = "));
    Serial.println(ACpm4_kWh);
    
    Serial.println(F("=================================================="));
    
    // AC_power_meter_5
    
    Serial.println(F("∙ POWER METER ENERGY TOTAL SUMMING"));

    m_rtu.sendReadHolding(PLC_ADDR,500,2);
    float ACpm5_VoltA = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_5 Volt A = "));
    Serial.println(ACpm5_VoltA);
    
    m_rtu.sendReadHolding(PLC_ADDR,502,2);
    float ACpm5_VoltB = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_5 Volt B = "));
    Serial.println(ACpm5_VoltB);
    
    m_rtu.sendReadHolding(PLC_ADDR,504,2);
    float ACpm5_VoltC = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_5 Volt C = "));
    Serial.println(ACpm5_VoltC);
    
    m_rtu.sendReadHolding(PLC_ADDR,506,2);
    float ACpm5_AmpA = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_5 Amp A = "));
    Serial.println(ACpm5_AmpA);
    
    m_rtu.sendReadHolding(PLC_ADDR,508,2);
    float ACpm5_AmpB = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_5 Amp B = "));
    Serial.println(ACpm5_AmpB);
    
    m_rtu.sendReadHolding(PLC_ADDR,510,2);
    float ACpm5_AmpC = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_5 Amp C = "));
    Serial.println(ACpm5_AmpC);
    
    m_rtu.sendReadHolding(PLC_ADDR,512,2);
    float ACpm5_Wtotal = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_5 Watt total = "));
    Serial.println(ACpm5_Wtotal);
    
    m_rtu.sendReadHolding(PLC_ADDR,514,2);
    float ACpm5_kWh = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("AC_power_meter_5 kWatt Hour = "));
    Serial.println(ACpm5_kWh);
    
    Serial.println(F("=================================================="));
    
    // DC_power_meter_1
    
    Serial.println(F("▲ DC_POWER_METER NO.1"));

    m_rtu.sendReadHolding(PLC_ADDR,600,2);
    float DCpm1_Volt = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_1 Volt = "));
    Serial.println(DCpm1_Volt);
    
    m_rtu.sendReadHolding(PLC_ADDR,602,2);
    float DCpm1_Amp = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_1 AMP = "));
    Serial.println(DCpm1_Amp);
    
    m_rtu.sendReadHolding(PLC_ADDR,604,2);
    float DCpm1_Watt = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_1 WATT = "));
    Serial.println(DCpm1_Watt);
    
    m_rtu.sendReadHolding(PLC_ADDR,606,2);
    float DCpm1_kWh = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_1 KWH = "));
    Serial.println(DCpm1_kWh);
    
    m_rtu.sendReadHolding(PLC_ADDR,608,2);
    float DCpm1_AmpHour = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_1 Amp_HOUR = "));
    Serial.println(DCpm1_AmpHour);
    
    Serial.println(F("=================================================="));
    
    // DC_power_meter_2
    
    Serial.println(F("▲ DC_POWER_METER NO.2"));

    m_rtu.sendReadHolding(PLC_ADDR,700,2);
    float DCpm2_Volt = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_2 Volt = "));
    Serial.println(DCpm2_Volt);
    
    m_rtu.sendReadHolding(PLC_ADDR,702,2);
    float DCpm2_Amp = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_2 AMP = "));
    Serial.println(DCpm2_Amp);
    
    m_rtu.sendReadHolding(PLC_ADDR,704,2);
    float DCpm2_Watt = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_2 WATT = "));
    Serial.println(DCpm2_Watt);
    
    m_rtu.sendReadHolding(PLC_ADDR,706,2);
    float DCpm2_kWh = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_2 KWH = "));
    Serial.println(DCpm2_kWh);
    
    m_rtu.sendReadHolding(PLC_ADDR,708,2);
    float DCpm2_AmpHour = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_2 Amp_HOUR = "));
    Serial.println(DCpm2_AmpHour);
    
    Serial.println(F("=================================================="));
    
    // DC_power_meter_3
    
    Serial.println(F("▲ DC_POWER_METER NO.3"));

    m_rtu.sendReadHolding(PLC_ADDR,800,2);
    float DCpm3_Volt = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_3 Volt = "));
    Serial.println(DCpm3_Volt);
    
    m_rtu.sendReadHolding(PLC_ADDR,802,2);
    float DCpm3_Amp = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_3 AMP = "));
    Serial.println(DCpm3_Amp);
    
    m_rtu.sendReadHolding(PLC_ADDR,804,2);
    float DCpm3_Watt = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_3 WATT = "));
    Serial.println(DCpm3_Watt);
    
    m_rtu.sendReadHolding(PLC_ADDR,806,2);
    float DCpm3_kWh = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_3 KWH = "));
    Serial.println(DCpm3_kWh);
    
    m_rtu.sendReadHolding(PLC_ADDR,808,2);
    float DCpm3_AmpHour = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_3 Amp_HOUR = "));
    Serial.println(DCpm3_AmpHour);
    
    Serial.println(F("=================================================="));
    
    // DC_power_meter_4
    
    Serial.println(F("▲ DC_POWER_METER NO.4"));

    m_rtu.sendReadHolding(PLC_ADDR,900,2);
    float DCpm4_Volt = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_4 Volt = "));
    Serial.println(DCpm4_Volt);
    
    m_rtu.sendReadHolding(PLC_ADDR,902,2);
    float DCpm4_Amp = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_4 AMP = "));
    Serial.println(DCpm4_Amp);
    
    m_rtu.sendReadHolding(PLC_ADDR,904,2);
    float DCpm4_Watt = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_4 WATT = "));
    Serial.println(DCpm4_Watt);
    
    m_rtu.sendReadHolding(PLC_ADDR,906,2);
    float DCpm4_kWh = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_4 KWH = "));
    Serial.println(DCpm4_kWh);
    
    m_rtu.sendReadHolding(PLC_ADDR,908,2);
    float DCpm4_AmpHour = m_rtu.recv_float(PLC_ADDR);
    Serial.print(F("DC_power_meter_4 Amp_HOUR = "));
    Serial.println(DCpm4_AmpHour);
    
    Serial.println(F("=================================================="));
    
    /***** Complete the packet *****/
    
    // Get timestamps
    DateTime dt;
    if(ntpUpdated) 
      dt = DateTime(timeClient.getEpochTime());
    else 
      dt = DateTime(rtcTime_st + (millis()/1000));

    // Build packet
    String packet_h = "SolarCellPlant,";
    packet_h += dt.timestamp(DateTime::TIMESTAMP_DATE);
    packet_h += ",";
    packet_h += dt.timestamp(DateTime::TIMESTAMP_TIME);
                           
    String packet_data_1 = "," + (String)ACpm1_VoltA  + "," + (String)ACpm1_VoltB +
                           "," + (String)ACpm1_VoltC  + "," + (String)ACpm1_AmpA +
                           "," + (String)ACpm1_AmpB   + "," + (String)ACpm1_AmpC +
                           "," + (String)ACpm1_Wtotal + "," + (String)ACpm1_kWh;
                           
    String packet_data_2 = "," + (String)ACpm2_VoltA  + "," + (String)ACpm2_VoltB +
                           "," + (String)ACpm2_VoltC  + "," + (String)ACpm2_AmpA +
                           "," + (String)ACpm2_AmpB   + "," + (String)ACpm2_AmpC +
                           "," + (String)ACpm2_Wtotal + "," + (String)ACpm2_kWh;
                           
    String packet_data_3 = "," + (String)ACpm3_VoltA  + "," + (String)ACpm3_VoltB +
                           "," + (String)ACpm3_VoltC  + "," + (String)ACpm3_AmpA +
                           "," + (String)ACpm3_AmpB   + "," + (String)ACpm3_AmpC +
                           "," + (String)ACpm3_Wtotal + "," + (String)ACpm3_kWh;
                           
    String packet_data_4 = "," + (String)ACpm4_VoltA  + "," + (String)ACpm4_VoltB +
                           "," + (String)ACpm4_VoltC  + "," + (String)ACpm4_AmpA +
                           "," + (String)ACpm4_AmpB   + "," + (String)ACpm4_AmpC +
                           "," + (String)ACpm4_Wtotal + "," + (String)ACpm4_kWh;
                           
    String packet_data_5 = "," + (String)ACpm5_VoltA  + "," + (String)ACpm5_VoltB +
                           "," + (String)ACpm5_VoltC  + "," + (String)ACpm5_AmpA +
                           "," + (String)ACpm5_AmpB   + "," + (String)ACpm5_AmpC +
                           "," + (String)ACpm5_Wtotal + "," + (String)ACpm5_kWh;
                           
    String packet_data_6 = "," + (String)DCpm1_Volt + "," + (String)DCpm1_Amp +
                           "," + (String)DCpm1_Watt + "," + (String)DCpm1_kWh +
                           "," + (String)DCpm1_AmpHour;
                           
    String packet_data_7 = "," + (String)DCpm2_Volt + "," + (String)DCpm2_Amp +
                           "," + (String)DCpm2_Watt + "," + (String)DCpm2_kWh +
                           "," + (String)DCpm2_AmpHour;
                           
    String packet_data_8 = "," + (String)DCpm3_Volt + "," + (String)DCpm3_Amp +
                           "," + (String)DCpm3_Watt + "," + (String)DCpm3_kWh +
                           "," + (String)DCpm3_AmpHour;
                           
    String packet_data_9 = "," + (String)DCpm4_Volt + "," + (String)DCpm4_Amp +
                           "," + (String)DCpm4_Watt + "," + (String)DCpm4_kWh +
                           "," + (String)DCpm4_AmpHour;

    // Send and save the packet When there's no INTERRUPT
    if(cntInt == 0) {
      Serial.println("► Send the packet");
      Serial.print(packet_h);
      Serial.print(packet_data_1);
      Serial.print(packet_data_2);
      Serial.print(packet_data_3);
      Serial.print(packet_data_4);
      Serial.print(packet_data_5);
      Serial.print(packet_data_6);
      Serial.print(packet_data_7);
      Serial.print(packet_data_8);
      Serial.print(packet_data_9);
      Serial.println();
      
      // send data via WiFi
      uint8_t udpPack[150];
      udp.beginPacket(HOST, String(PORT).toInt());
      
      packet_h.getBytes(udpPack, packet_h.length()+1);
      udp.write(udpPack, packet_h.length());
      
      packet_data_1.getBytes(udpPack, packet_data_1.length()+1);
      udp.write(udpPack, packet_data_1.length());
      
      packet_data_2.getBytes(udpPack, packet_data_2.length()+1);
      udp.write(udpPack, packet_data_2.length());
      
      packet_data_3.getBytes(udpPack, packet_data_3.length()+1);
      udp.write(udpPack, packet_data_3.length());
      
      packet_data_4.getBytes(udpPack, packet_data_4.length()+1);
      udp.write(udpPack, packet_data_4.length());
      
      packet_data_5.getBytes(udpPack, packet_data_5.length()+1);
      udp.write(udpPack, packet_data_5.length());
      
      packet_data_6.getBytes(udpPack, packet_data_6.length()+1);
      udp.write(udpPack, packet_data_6.length());
      
      packet_data_7.getBytes(udpPack, packet_data_7.length()+1);
      udp.write(udpPack, packet_data_7.length());
      
      packet_data_8.getBytes(udpPack, packet_data_8.length()+1);
      udp.write(udpPack, packet_data_8.length());
      
      packet_data_9.getBytes(udpPack, packet_data_9.length()+1);
      udp.write(udpPack, packet_data_9.length());
      
      udp.endPacket();
      memset(udpPack, 0, 150);  // clean buffer
      
      // save data in SD-Card
      if(SD.begin(cs)) {
        Serial.println("Save data to SDcard.");
        appendFile(SD, logFile, packet_h.c_str());
        appendFile(SD, logFile, packet_data_1.c_str());
        appendFile(SD, logFile, packet_data_2.c_str());
        appendFile(SD, logFile, packet_data_3.c_str());
        appendFile(SD, logFile, packet_data_4.c_str());
        appendFile(SD, logFile, packet_data_5.c_str());
        appendFile(SD, logFile, packet_data_6.c_str());
        appendFile(SD, logFile, packet_data_7.c_str());
        appendFile(SD, logFile, packet_data_8.c_str());
        appendFile(SD, logFile, packet_data_9.c_str());
        appendFile(SD, logFile, "\r\n");
      }
      else {
        Serial.println("SDcard is unavailable!");
      }
      Serial.println(F("=================================================="));
    }
    Serial.println();
    Serial.println();
  }
  
  // When ESP32 is interrupted by RTC
  if(cntInt > 0 || (millis() > 259200000UL)) {
    ESP.restart();    // restart ESP when got an interrupt
  }
}
