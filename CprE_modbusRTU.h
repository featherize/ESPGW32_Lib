#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <Arduino.h>
#include <Stream.h>

class CprE_modbusRTU {
	public:
		uint8_t buf[128];
		int buf_length();
		
		void initSerial(HardwareSerial &serial, int dirpin);
		uint8_t getError();
		String errorReport();
		void crc16_update(uint16_t &crc_holder, uint8_t byteIn);
		uint16_t crc16_gen(uint8_t* packet, int len);
		
		void sendpacket(uint8_t* packet, int length, bool auto_crc = false);
		void recv(uint8_t SS);			// read and store in <buf>
		
		void sendReadCoil(uint8_t SS, int start_addr, int reg_len);
		void sendReadDiscrete(uint8_t SS, int start_addr, int reg_len);
		void sendReadHolding(uint8_t SS, int start_addr, int reg_len);
		void sendReadInput(uint8_t SS, int start_addr, int reg_len);
		
		int8_t recv_byte(uint8_t SS);	// return only highest-order data
		long   recv_int(uint8_t SS);	// return all data in [long] format
		float  recv_float(uint8_t SS);	// return 4 bytes data in [float] format
		String recv_string(uint8_t SS);	// return all data in [String] format
		
	private:
		HardwareSerial* _serial;
		int _dirpin;
		int8_t indexMax = 0;
		int8_t indexPacket = 0;
		int8_t indexData = 0;
		int8_t lastIndexData = 0;
		uint8_t m_error = 0;
};

#endif

// Author note
// For future update : https://www.modbustools.com/modbus.html
