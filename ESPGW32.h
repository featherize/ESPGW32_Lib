#ifndef ESPGW32_H
#define ESPGW32_H

#include <Arduino.h>
#include "CprE_ModbusRTU.h"
#include "CprE_DS3231.h"
#include "CprE_NB_bc95.h"

#define SDA      26 
#define SCL      25 
#define RTCINT   27		// DS3231 interupt pin
#define DIRPIN   13		// RS485 direction pin
#define RXmax    32 
#define TXmax    33 	// Rx,Tx pin connect to MAX232 and MAX485
#define Uno8     22 	
#define Uno9     23 	// Rx,Tx pin connect to pin 9,8(uno) of Arduino shield

#endif
