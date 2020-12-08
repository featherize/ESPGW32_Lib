/***********************************************************************
 * Project       : Weather-Station
 * Board Select  : ESP32vn IoT Uno
 * Description   : Send <Wind Speed>,<Wind Direction> and <EnergyUsage> 
 *                 packets to server via WiFi every 5 minutes (Also save
 *                 log in SDcard) and restart itself everyday at 6 am
 * Packet format : <projectName>,<date>,<time>,<w-speed>,<w-direction>,
 *                 <e-volt_A>,<e-current_A>,<e-power_A>,<e-energy_A>,
 *                 <e-volt_B>,<e-current_B>,<e-power_B>,<e-energy_B>
 * Last update   : 23 SEP 2020
 * Author        : CprE13-KMUTNB
 ***********************************************************************
 * Note : 
 * - Move both JUMPERs to RS485 position.
 * - Wiring with YGC-FS (RED:12V),(BLACK:A+),(YELLOW:B-),(GREEN:GND)
 * - Wiring with YGC-FX (RED:12V),(BLACK:A+),(YELLOW:B-),(GREEN:GND)
 * - Install YGC-FX position correctly for correct data
 * - SDM120CT-MV Wiring Guide: 
 *   http://www.etteam.com/productDIN/SDM120CT-MV/%E0%B8%81%E0%B8%B2%E0%B8%A3%E0%B8%95%E0%B9%88%E0%B8%AD%E0%B9%83%E0%B8%8A%E0%B9%89%E0%B8%87%E0%B8%B2%E0%B8%99_SMD120CT-MV.pdf
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

#define YGCFS_ADDR     4
#define YGCFX_ADDR     5
#define SDM_ADDR_A     1
#define SDM_ADDR_B     2

CprE_DS3231 rtc(SDA,SCL);
CprE_modbusRTU m_rtu;
WiFiUDP udp;
NTPClient timeClient(udp, 25200);

int cntInt = 0;
unsigned long prev_t = 0;
unsigned long interval = 300000;   // interval time of each packet

int sck = 21;    // SPI connect to SDcard module
int miso = 19;
int mosi = 18;
int cs = 14;
const char *logFile = "/datalogs.txt";
bool ntpUpdated = false;
unsigned long rtcTime_st;    // RTC time when setup() is running

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
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void IRAM_ATTR isr() {
  Serial.println("*** INTERRUPT! ***");
  ++cntInt;   // increase <cntInt> when got interrupted
}

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600,SERIAL_8N1,RXmax,TXmax);   // connect to RS485 device
  m_rtu.initSerial(Serial1, DIRPIN);
  Serial.println("# START");
  Serial.println("# IoT PROJECT   : Weather Station");
  Serial.println("# Modbus Device : Wind Speed, Wind Direction and 2 Power Meters");
  Serial.println("# Interface     : RS485");
  Serial.println("# Database      : Cloud DB and SDcard");

  // init WIFI
  Serial.println("# Initialize WiFi...");
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
    Serial.print("Connected to ");
    Serial.println(SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  udp.begin(1234);	// open udp at port 1234

  // init RTC
  Serial.println("# Initialize RTC...");
  rtc.clearFlag();
  rtc.disableAlarm();
  timeClient.begin();
  if(timeClient.update()) {
    Serial.println("Update time from NTP-Server to RTC");
    rtc.adjust(DateTime(timeClient.getEpochTime()));
	ntpUpdated = true;
  }
  rtc.setAlarm1(6,00,00,0,false,'D');  // interrupt every day at 6am
  rtc.enableAlarm(1);
  rtcTime_st = rtc.now().unixtime();
  Serial.println("Time   = " + rtc.currentTime());
  Serial.println("Alarm1 = " + rtc.getAlarm1());

  // attach interrupt pin
  Serial.println("# Attach interrupt pin to RTC's <INT> pin");
  pinMode(RTCINT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTCINT), isr, FALLING);

  // Initialize SD card Module
  bool sd_en = true;
  Serial.print("# Initialize SD card...");
  SPI.begin(sck, miso, mosi, cs);
  if(!SD.begin(cs)) {
    Serial.println("Card Mount Failed");
    sd_en = false;
  }
  uint8_t cardType = SD.cardType();
  if((cardType == CARD_NONE) && sd_en) {
    Serial.println("No SD card attached");
    sd_en = false;
  }
  if(sd_en) {
    Serial.println("DONE");
    File file = SD.open(logFile);
    if(!file) {
      Serial.println("LogFile doesn't exist");
      Serial.println("Creating file...");
      // write header of file
      const char *header = "projectName,date,time,"
                           "w-speed,w-direction,"
                           "e-volt_A,e-current_A,e-power_A,e-energy_A,"
                           "e-volt_B,e-current_B,e-power_B,e-energy_B\r\n";
      writeFile(SD, logFile, header);
    }
    else {
      Serial.println("File already exists");
    }
    file.close();
  }
  if(!(wifi_en || sd_en)) {
    Serial.println("# Cannot connect to both WiFi and SDCard");
    Serial.println("# Restart ESP");
    Serial.println("**************************************************");
    ESP.restart();
  }
  Serial.println("**************************************************");
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
	
    m_rtu.sendReadHolding(YGCFS_ADDR,0,1);      // read wind speed
    float windspeed = ((float)m_rtu.recv_int(YGCFS_ADDR))/10;
    Serial.println("Wind speed                    : " + (String)windspeed + " m/s");
    m_rtu.sendReadHolding(YGCFX_ADDR,0,1);      // read wind direction
    int winddir = m_rtu.recv_int(YGCFX_ADDR);
    Serial.println("Wind direction (ref\u21bb)     : " + (String)winddir + "\u00B0");
    m_rtu.sendReadInput(SDM_ADDR_A,0,2);      // read voltage
    float voltA = m_rtu.recv_float(SDM_ADDR_A);
    Serial.println("Voltage (meter_A)             : " + (String)voltA + " Volts");
    m_rtu.sendReadInput(SDM_ADDR_A,6,2);        // read current
    float currentA = m_rtu.recv_float(SDM_ADDR_A);
    Serial.println("Current (meter_A)             : " + (String)currentA + " Amps");
    m_rtu.sendReadInput(SDM_ADDR_A,12,2);       // read active power
    float actPowerA = m_rtu.recv_float(SDM_ADDR_A);
    Serial.println("Active Power (meter_A)        : " + (String)actPowerA + " Watts");
    m_rtu.sendReadInput(SDM_ADDR_A,342,2);      // read total active energy
    float totalActA = m_rtu.recv_float(SDM_ADDR_A);
    Serial.println("Total Active Energy (meter_A) : " + (String)totalActA + " kWh");
    m_rtu.sendReadInput(SDM_ADDR_B,0,2);      // read voltage
    float voltB = m_rtu.recv_float(SDM_ADDR_B);
    Serial.println("Voltage (meter_B)             : " + (String)voltB + " Volts");
    m_rtu.sendReadInput(SDM_ADDR_B,6,2);        // read current
    float currentB = m_rtu.recv_float(SDM_ADDR_B);
    Serial.println("Current (meter_B)             : " + (String)currentB + " Amps");
    m_rtu.sendReadInput(SDM_ADDR_B,12,2);       // read active power
    float actPowerB = m_rtu.recv_float(SDM_ADDR_B);
    Serial.println("Active Power (meter_B)        : " + (String)actPowerB + " Watts");
    m_rtu.sendReadInput(SDM_ADDR_B,342,2);      // read total active energy
    float totalActB = m_rtu.recv_float(SDM_ADDR_B);
    Serial.println("Total Active Energy (meter_B) : " + (String)totalActB + " kWh");

    DateTime dt;
    if(ntpUpdated) 
      dt = DateTime(timeClient.getEpochTime());
    else 
      dt = DateTime(rtcTime_st + (millis()/1000));
  
    String packet = "Weather-Station,"; 
    packet += dt.timestamp(DateTime::TIMESTAMP_DATE);
    packet += ",";
    packet += dt.timestamp(DateTime::TIMESTAMP_TIME);
    packet += ",";
    packet += (String)windspeed;
    packet += ",";
    packet += (String)winddir;
    packet += ",";
    packet += (String)voltA;
    packet += ",";
    packet += (String)currentA;
    packet += ",";
    packet += (String)actPowerA;
    packet += ",";
    packet += (String)totalActA;
    packet += ",";
    packet += (String)voltB;
    packet += ",";
    packet += (String)currentB;
    packet += ",";
    packet += (String)actPowerB;
    packet += ",";
    packet += (String)totalActB;
    
    // Send the packet When there's no INTERRUPT 
    if(cntInt == 0) {
      Serial.print("packet send : ");
      Serial.println(packet);
      // send data via WiFi
      uint8_t udpPack[150];
      packet.getBytes(udpPack, packet.length()+1);
      udp.beginPacket(HOST, String(PORT).toInt());
      udp.write(udpPack, packet.length());
      udp.endPacket();
      memset(udpPack, 0, 150);  // clean buffer
      // save data in SD-Card
      if(SD.begin(cs)) {
        Serial.println("Save data to SDcard...");
        packet += "\r\n";
        appendFile(SD, logFile, packet.c_str());
      }
      else {
        Serial.println("SDcard is unavailable!");
      }
      Serial.println("<---------------------------------------->");
    }
  }
  
  // When ESP32 is interrupted by RTC
  if(cntInt > 0) {
    ESP.restart();    // restart ESP when got an interrupt
  }
}
