#include "RTClib.h"
#include "ESPGW32.h"

CprE_DS3231 rtc(SDA,SCL);
int cntInt = 0;

void IRAM_ATTR isr() {
    Serial.println("*** INTERRUPT! ***");
    ++cntInt;   // increase <cntInt> when got interrupted
}

void setup() {
  Serial.begin(9600);
  Serial.println("BEGIN");
  rtc.clearFlag();
  rtc.disableAlarm();
  
  Serial.println("- old setting");
  Serial.println("Time   : " + rtc.currentTime());
  Serial.println("Alarm1 : " + rtc.getAlarm1());
  Serial.println("Alarm2 : " + rtc.getAlarm2());
  
  Serial.println("- init RTC");
  bool t_err = rtc.lostPower();
  Serial.print("lostPower = ");
  Serial.println(t_err? "true":"false");
  if(t_err) {
    Serial.println("Set RTC time!");
  // rtc.adjust(DateTime(2019, 12, 31, 23, 59, 0)); // set 31/12/2019 23:59:00
    rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));
  }
  rtc.setAlarm1(23,59,00,0,false,'h');  // h:m:s = 23:59:00 , every 'h'our
  rtc.setAlarm2(00,01,0,false,'m');     // h:m   = 00:01    , every 'm'inute
                                        //                          's'econd (Alarm1 only)
                                        //                          'D'ay
//  rtc.enableAlarm(1);   // uncomment to enable Alarm 1
//  rtc.enableAlarm(2);   // uncomment to enable Alarm 2

  Serial.println("- new setting");
  Serial.println("Time   : " + rtc.currentTime());
  Serial.println("Alarm1 : " + rtc.getAlarm1());
  Serial.println("Alarm2 : " + rtc.getAlarm2());

  // config interrupt pin of controller
  // Hardware: RTCINT is pin that attach to INT pin of DS3231
  pinMode(RTCINT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTCINT), isr, FALLING);
}

void loop() {
  // When there is an interrupt. <cntInt> will increase.
  if(cntInt > 0) {  
    Serial.println("Time : " + rtc.currentTime());
    rtc.clearFlag();  // Always clear flag after got interrupted
    cntInt = 0;
  }
}
