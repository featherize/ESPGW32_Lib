#include "CprE_modbusRTU.h"

int CprE_modbusRTU::buf_length() {
	return indexMax;
}

void CprE_modbusRTU::initSerial(Stream &serial, int dirpin) {
	_serial = &serial;
	_dirpin = dirpin;
	pinMode(dirpin, OUTPUT);
}

uint8_t CprE_modbusRTU::getError() {
	return m_error;
}

String CprE_modbusRTU::errorReport() {
	switch(m_error) {
		case 0:
			return "NONE";
		case 1:
			return "TIMEOUT";
		case 2:
			return "CANNOT FIND HEADER OF PACKET";
		case 3:
			return "DAMAGED PACKET";
		case 4:
			return "CRC INCORRECT";
		case 5:
			return "EXCEPTION RESPONSE";
		case 6:
			return "NO DATA FROM THIS PACKET";
		default:
			return "UNKNOWN";
	}
}

void CprE_modbusRTU::crc16_update(uint16_t &crc_holder, uint8_t byteIn) {
	crc_holder ^= byteIn;
	for (uint8_t i=0; i<8; i++){
		if(crc_holder & 1) 
			crc_holder = (crc_holder >> 1) ^ 0xA001;
		else 
			crc_holder >>= 1;
	}
}

uint16_t CprE_modbusRTU::crc16_gen(uint8_t* packet, int len) {
	uint16_t crc = 0xFFFF;
	for(uint8_t i=0; i<len; i++) {
		crc16_update(crc, packet[i]);
	}
	return crc;
}

void CprE_modbusRTU::sendpacket(uint8_t* packet, int length, bool auto_crc) {
	uint16_t crcA, crcB;
	if(auto_crc) {
		uint16_t crc = crc16_gen(packet,length);
		crcA = crc & 0xFF;
		crcB = crc >> 8;
	}
	
	uint32_t br = _serial->baudRate();	// Get baudrate of _serial
	uint16_t gap_t = (240000UL / br);	// Silent time (ms) before and after sending frame
	
	digitalWrite(_dirpin,HIGH);
	delay(gap_t);
	_serial->write(packet,length);
	if(auto_crc) {
		_serial->write(crcA);
		_serial->write(crcB);
	}
	delay(gap_t);
	digitalWrite(_dirpin,LOW);
}

void CprE_modbusRTU::recv(uint8_t SS) {
	unsigned long prev_t = millis();
	while(_serial->available() == 0) {
		if(millis() - prev_t > 3000) {
			m_error = 1;			// TIMEOUT
			return;
		}
	}
	int8_t indexNow;
	indexMax = 0;
	indexPacket = -1;
	indexData = 0;
	lastIndexData = 0;
	m_error = 0;
	
	prev_t = millis();
	while(_serial->available() > 0) {
		buf[indexMax++] = _serial->read();
		while(_serial->available() == 0) {
			if(millis()-prev_t > 500) {
				break;				// no more data
			}
		}
	}
	
	// Check read response packet
 	do {
		indexNow = indexPacket+1;
		while(buf[indexNow] != SS) {
			indexNow++;
			if(indexNow >= indexMax) {
				indexData = 0;
				lastIndexData = 0;
				if(m_error == 0) 
					m_error = 2;	// CANNOT FIND HEADER OF PACKET
				return;
			}
		}
		indexPacket = indexNow++;
		uint8_t dataSize;
		if(buf[indexNow] <= 0x04) {
			dataSize = buf[++indexNow];
			if(indexNow+dataSize+3 != indexMax) {
				m_error = 3;		// DAMAGED PACKET
				continue;
			}
			indexData = ++indexNow;
			indexNow += dataSize;
			lastIndexData = indexNow-1;
		}
		else if(buf[indexNow] > 0x80) {
			m_error = 5;			// EXCEPTION RESPONSE
			continue;
		}
		else {
			m_error = 6;			// NO DATA FROM THIS PACKET 
			continue;
		}
		if(indexNow+1 >= indexMax) {
			m_error = 3;			// DAMAGED PACKET
			return;
		}
		uint16_t crc = 0xFFFF;
		for(uint8_t i=indexPacket; i<indexNow; i++) {
			crc16_update(crc, buf[i]);
		}
		uint8_t crcA = crc & 0xFF;
		uint8_t crcB = crc >> 8;
		if((crcA != buf[indexNow]) || (crcB != buf[indexNow+1])) {
			indexPacket = 0;
			indexData = 0;
			lastIndexData = 0;
			m_error = 4;			// CRC INCORRECT
			continue;
		}
		m_error = 0;
	}
	while(m_error);
}

void CprE_modbusRTU::sendReadCoil(uint8_t SS, int start_addr, int reg_len) {
	uint8_t packet[6] = {SS, 0x01, start_addr>>8, start_addr, reg_len>>8, reg_len};
	sendpacket(packet, 6, true);
}

void CprE_modbusRTU::sendReadDiscrete(uint8_t SS, int start_addr, int reg_len) {
	uint8_t packet[6] = {SS, 0x02, start_addr>>8, start_addr, reg_len>>8, reg_len};
	sendpacket(packet, 6, true);
}

void CprE_modbusRTU::sendReadHolding(uint8_t SS, int start_addr, int reg_len) {
	uint8_t packet[6] = {SS, 0x03, start_addr>>8, start_addr, reg_len>>8, reg_len};
	sendpacket(packet, 6, true);
}

void CprE_modbusRTU::sendReadInput(uint8_t SS, int start_addr, int reg_len) {
	uint8_t packet[6] = {SS, 0x04, start_addr>>8, start_addr, reg_len>>8, reg_len};
	sendpacket(packet, 6, true);
}

int8_t CprE_modbusRTU::recv_byte(uint8_t SS) {
	recv(SS);
	if(!getError()) {
		return buf[indexData];
	}
	else 
		return -1;
}

long CprE_modbusRTU::recv_int(uint8_t SS) {
	recv(SS);
	if(!getError()) {
		long val = 0;
		for(int i=indexData; i<=lastIndexData; i++) {
			val = val << 8;
			val = val ^ buf[i];
		}
		return val;
	}
	else 
		return -1;
}

float CprE_modbusRTU::recv_float(uint8_t SS) {
	recv(SS);
	if(!getError()) {
		typedef union {
			uint8_t num[4];
			float f;
		} myFloat;
		
		myFloat data;
		for(uint8_t i=0; i<4; i++) {
			data.num[i] = buf[indexData+i];
		}
		return data.f;
	}
	else 
		return -1.0;
}

String CprE_modbusRTU::recv_string(uint8_t SS) {
	recv(SS);
	if(!getError()) {
		String str = "";
		for(uint8_t i=indexData; i<=lastIndexData; i++) {
			str += char(buf[i]);
		}
	}
	else 
		return "";
}
 
 
 
 
 
 
 
 
 
 
