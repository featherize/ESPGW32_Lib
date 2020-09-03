// MicroSD Card Adapter
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "FSfunc.h"

// define pins
int sck = 21;
int miso = 19;
int mosi = 18;
int cs = 14;

void setup() {
  Serial.begin(9600);
  Serial.println("Initialize SD card...");
  
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
  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC) 
    Serial.println("MMC");
  else if(cardType == CARD_SD) 
    Serial.println("SDSC");
  else if(cardType == CARD_SDHC) 
    Serial.println("SDHC");
  else 
    Serial.println("UNKNOWN");

  // Query Data
  uint64_t cardSize = SD.cardSize() / (1024*1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  listDir(SD, "/", 0);
  createDir(SD, "/mydir");
  listDir(SD, "/", 0);
  removeDir(SD, "/mydir");
  listDir(SD, "/", 2);
  writeFile(SD, "/hello.txt", "Hello ");
  appendFile(SD, "/hello.txt", "World!\n");
  readFile(SD, "/hello.txt");
  deleteFile(SD, "/foo.txt");
  renameFile(SD, "/hello.txt", "/foo.txt");
  readFile(SD, "/foo.txt");
  testFileIO(SD, "/test.txt");
  listDir(SD, "/", 0);
  
  deleteFile(SD, "/foo.txt");
  deleteFile(SD, "/test.txt");
  listDir(SD, "/", 0);
  
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
}

void loop() {
  // put your main code here, to run repeatedly:

}
