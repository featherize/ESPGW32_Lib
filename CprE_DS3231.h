#ifndef CPRE_DS_3231_H
#define CPRE_DS_3231_H

#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"

class CprE_DS3231 {
	public:
		CprE_DS3231(int sda, int scl);
		uint8_t bcd2bin(uint8_t val) { return val-6 * (val>>4); }
		uint8_t bin2bcd(uint8_t val) { return val+6 * (val/10); }
		uint8_t readAddr(byte reg);
		void writeAddr(byte reg, byte val);
		String currentTime();
		String getAlarm1();
		String getAlarm2();
		void setAlarm1(byte h, byte m, byte s, byte D, bool Dy, char mode);
		void setAlarm2(byte h, byte m, byte D, bool Dy, char mode);
		void enableAlarm(uint8_t id);
		void disableAlarm();
		void clearFlag();
		
		bool lostPower();
		void adjust(const DateTime& dt);
		
	private:
		RTC_DS3231 _rtclib;
};

#endif
