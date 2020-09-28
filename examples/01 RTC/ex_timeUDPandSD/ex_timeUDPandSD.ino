// Test query DS3231(RTC) and NTP time update
// Save data to SDcard every 5 minutes
// Restart everyday at 11 am

#include "ESPGW32.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SSID        ""    // WIFI name
#define PASS        ""    // WIFI password

#define HOST        ""      // server ip
#define PORT        ""      // server udp port

#define SEND_TO_WIFI    0
#define SEND_TO_SD      0

CprE_DS3231 rtc(SDA,SCL);
WiFiUDP udp;
NTPClient timeClient(udp, 25200);

int cntInt = 0;
unsigned long prev_t = 0;
unsigned long interval = 300000;   // interval time of each packet
uint16_t pno = 1;

int sck = 21;    // SPI connect to SDcard module
int miso = 19;
int mosi = 18;
int cs = 14;
const char *logFile = "/timelogs.txt";
bool ntpUpdated = false;
unsigned long rtcTime_st;   // RTC time when ESP setup running

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
  Serial.println("# START");
  Serial.println("# IoT PROJECT   : RTC-TimeQuery");
  Serial.println("# Interface     : I2C");
  Serial.println("# Database      : SDcard");

  // init WIFI
  Serial.println("# Initialize WiFi...");
  WiFi.begin(SSID, PASS);
  uint8_t waitwifi = 20;
  while (WiFi.status() != WL_CONNECTED) { // wait for connection
    delay(500);
    Serial.print(".");
    waitwifi--;
    if(waitwifi <= 0) {
      Serial.println("Connect Fail!");
      break;
    }
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  udp.begin(1234);  // port 1234

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
  rtc.setAlarm1(11,00,00,0,false,'D');  // interrupt every day at 11am
  rtc.enableAlarm(1);
  rtcTime_st = rtc.now().unixtime();
  Serial.println("Time   = " + rtc.currentTime());
  Serial.println("Alarm1 = " + rtc.getAlarm1());

  // attach interrupt pin
  Serial.println("# Attach interrupt pin to RTC's <INT> pin");
  pinMode(RTCINT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTCINT), isr, FALLING);

  // Initialize SD card Module
  Serial.print("# Initialize SD card...");
  SPI.begin(sck, miso, mosi, cs);
  if(!SD.begin(cs)) {
    Serial.println("Card Mount Failed");
  }
  else {
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE) {
      Serial.println("No SD card attached");
    }
    else {
      File file = SD.open(logFile);
      if(!file) {
        Serial.println("LogFile doesn't exist");
        Serial.println("Creating file...");
        // write header of file
        const char *header = "Project,"
                             "EpochTimeNTP,"
                             "EpochTime,"
                             "Date(YYYY-MM-DD),"
                             "Time(hh:mm:ss),"
                             "useTimeNTP?,"
                             "p_no\r\n";
        writeFile(SD, logFile, header);
      }
      else {
        Serial.println("File already exists");
      }
      file.close();
    }
  }
  Serial.println("***********************************************************************");
  Serial.println("Format: Project, EpochTimeNTP, EpochTime, Date, Time, useTimeNTP?, p_no");
  Serial.println("***********************************************************************");
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

    DateTime dt;
    if(ntpUpdated) 
      dt = DateTime(timeClient.getEpochTime());
    else 
      dt = DateTime(rtcTime_st + (millis()/1000));
  
    String packet = "RTC-TimeQuery,";                   // Project
    packet += (String)timeClient.getEpochTime();        // EpochTimeNTP
    packet += ",";
    packet += (String)dt.unixtime();                    // EpochTime
    packet += ",";
    packet += dt.timestamp(DateTime::TIMESTAMP_DATE);   // Date
    packet += ",";
    packet += dt.timestamp(DateTime::TIMESTAMP_TIME);   // Time
    packet += ",";
    packet += (String)ntpUpdated;                       // use ntpTime:rtcTime
    packet += ",";
    packet += (String)pno++;                            // p_no

    // Send the packet when there is no INTERRUPT 
    if(cntInt == 0) {
      Serial.print("packet send : ");
      Serial.println(packet);
	  if(SEND_TO_WIFI) {
        // send data via WiFi
        uint8_t udpPack[150];
        packet.getBytes(udpPack, packet.length()+1);
        udp.beginPacket(HOST, String(PORT).toInt());
        udp.write(udpPack, packet.length());
        udp.endPacket();
        memset(udpPack, 0, 150);  // clean buffer
	  }
	  if(SEND_TO_SD) {
        // save data in SD-Card
        if(SD.begin(cs)) {
          Serial.println("Save data to SDcard...");
          packet += "\r\n";
          appendFile(SD, logFile, packet.c_str());
        }
        else {
          Serial.println("SDcard is not available!");
        }
      }
      Serial.println("<-------------------------------------------------------->");
    }
  } // end time interval

  // When ESP32 is interrupted by RTC
  if(cntInt > 0) {
    ESP.restart();    // restart ESP when got an interrupt
  }
}
