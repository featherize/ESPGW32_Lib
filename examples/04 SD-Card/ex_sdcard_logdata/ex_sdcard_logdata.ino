// MicroSD Card Adapter
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// define pins
int sck = 21;
int miso = 19;
int mosi = 18;
int cs = 5;
// logging fileName & interval time
String logFile = "/datalog.txt";
unsigned long prev_t = 0;
unsigned long interval = 5000;

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

void setup() {
  Serial.begin(9600);
  Serial.print("Initialize SD card...");
  
  // Initialize SD card Module
  SPI.begin(sck, miso, mosi, cs);
  if(!SD.begin(cs)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;  
  }
  Serial.println("DONE");

  File file = SD.open(logFile);
  if(!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    // write header of file
    writeFile(SD, logFile.c_str(), "\"project\",\"node\",\"value_1\",\"value_2\"\r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();
  Serial.println("########################################");
}

void loop() {
  static unsigned long count = 1;
  unsigned long curr_t = millis();

  if(curr_t-prev_t > interval || prev_t == 0) {
    prev_t = curr_t;
    long numRandom = random(10, 50); // min, max
    String packet = "\"counter\",\"device1\",";
    packet += (String)count;
    packet += ",";
    packet += (String)numRandom;
    
    Serial.print("Save data: ");
    Serial.println(packet);
    packet += "\r\n";
    appendFile(SD, logFile.c_str(), packet.c_str());
    ++count;
    Serial.println("----------------------------------------");
  }
}
