#include "CprE_DS3231.h"

CprE_DS3231::CprE_DS3231(int sda, int scl){
	Wire.begin(sda,scl);
	Wire.beginTransmission(DS3231_ADDRESS);
	if(Wire.endTransmission() != 0) 
		Serial.println(F("*** DS3231 not found!"));
}

uint8_t CprE_DS3231::readAddr(byte reg) {
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(reg);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDRESS, (byte)1);
	return Wire.read();
}

void CprE_DS3231::writeAddr(byte reg, byte val) {
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(reg);
	Wire.write(val);
	Wire.endTransmission();
}

DateTime CprE_DS3231::now() {
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write((byte)0);
  Wire.endTransmission();

  Wire.requestFrom(DS3231_ADDRESS, (byte)7);
  uint8_t ss = bcd2bin(Wire.read() & 0x7F);
  uint8_t mm = bcd2bin(Wire.read());
  uint8_t hh = bcd2bin(Wire.read());
  Wire.read();
  uint8_t d = bcd2bin(Wire.read());
  uint8_t m = bcd2bin(Wire.read());
  uint16_t y = bcd2bin(Wire.read()) + 2000;

  return DateTime(y, m, d, hh, mm, ss);
}

String CprE_DS3231::currentTime() {
	uint8_t s, m, h;
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write((byte)0);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDRESS, (byte)3);
	s = bcd2bin(Wire.read());
	m = bcd2bin(Wire.read());
	h = bcd2bin(Wire.read());
	String t = "";
	t += (String)h;
	t += ":";
	t += (String)m;
	t += ":";
	t += (String)s;
	return t;
}

String CprE_DS3231::getAlarm1() {
	uint8_t s, m, h, D, mark;
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write((byte)7);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDRESS, (byte)4);
	s = Wire.read();
	m = Wire.read();
	h = Wire.read();
	D = Wire.read();
	mark = ((D&0x80)>>4)^((h&0x80)>>5)^((m&0x80)>>6)^((s&0x80)>>7);
	String alm = "";
	alm += (D&0x40)? "day ":"date ";
	alm += bcd2bin(D&~0xC0);
	alm += " / time ";
	alm += bcd2bin(h&~0x80);
	alm += ":";
	alm += bcd2bin(m&~0x80);
	alm += ":";
	alm += bcd2bin(s&~0x80);
	alm += " / mode ";
	alm += String(mark,BIN);
	return alm;
}

String CprE_DS3231::getAlarm2() {
	uint8_t m, h, D, mark;
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write((byte)0x0B);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDRESS, (byte)3);
	m = Wire.read();
	h = Wire.read();
	D = Wire.read();
	mark = ((D&0x80)>>5)^((h&0x80)>>6)^((m&0x80)>>7);
	String alm = "";
	alm += (D&0x40)? "day ":"data ";
	alm += bcd2bin(D&~0xC0);
	alm += " / time ";
	alm += bcd2bin(h&~0x80);
	alm += ":";
	alm += bcd2bin(m&~0x80);
	alm += " / mode ";
	alm += String(mark,BIN);
	return alm;
}

void CprE_DS3231::setAlarm1(byte h, byte m, byte s, byte D, bool Dy, char mode) {
	byte a1m1 = 0x80, a1m2 = 0x80, a1m3 = 0x80, a1m4 = 0x80;
	switch(mode) {
		case 'M': a1m4 = 0x00;	// Alarm when D,h,m,s match
		case 'D': a1m3 = 0x00;	// Alarm when h,m,s match
		case 'h': a1m2 = 0x00;	// Alarm when m,s match
		case 'm': a1m1 = 0x00;	// Alarm when s match
		case 's': break;		// Alarm every second
	}
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write((byte)7);
	Wire.write(bin2bcd(s)|a1m1);
	Wire.write(bin2bcd(m)|a1m2);
	Wire.write(bin2bcd(h)|a1m3);
	if(Dy) 
		Wire.write(bin2bcd(D)|0x40|a1m4);
	else 
		Wire.write(bin2bcd(D)|a1m4);
	Wire.endTransmission();
}

void CprE_DS3231::setAlarm2(byte h, byte m, byte D, bool Dy, char mode) {
	byte a2m2 = 0x80, a2m3 = 0x80, a2m4 = 0x80;
	switch(mode) {
		case 'M': a2m4 = 0x00;	// Alarm when D,h,m match
		case 'D': a2m3 = 0x00;	// Alarm when h,m match
		case 'h': a2m2 = 0x00;	// Alarm when m match
		case 'm': break;		// Alarm every minute
	}
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write((byte)0x0B);
	Wire.write(bin2bcd(m)|a2m2);
	Wire.write(bin2bcd(h)|a2m3);
	if(Dy) 
		Wire.write(bin2bcd(D)|0x40|a2m4);
	else 
		Wire.write(bin2bcd(D)|a2m4);
	Wire.endTransmission();
}

void CprE_DS3231::enableAlarm(uint8_t id) {
	if(id > 2 || id < 1) 
		return;
	byte ctrl = readAddr(DS3231_CONTROL);
	ctrl |= 0x04|id;
	writeAddr(DS3231_CONTROL, ctrl);
}

void CprE_DS3231::disableAlarm() {
	byte ctrl = readAddr(DS3231_CONTROL);
	ctrl &= ~0x03;
	writeAddr(DS3231_CONTROL, ctrl);
}

void CprE_DS3231::clearFlag() {
	uint8_t sta;
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(DS3231_STATUSREG);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDRESS, (byte)1);
	sta = Wire.read();
	sta &= ~0x03;
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(DS3231_STATUSREG);
	Wire.write(sta);
	Wire.endTransmission();
}

bool CprE_DS3231::lostPower() {
	return _rtclib.lostPower();
}

void CprE_DS3231::adjust(const DateTime& dt) {
	_rtclib.adjust(dt);
}









