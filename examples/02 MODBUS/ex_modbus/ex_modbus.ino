#include "ESPGW32.h"

CprE_modbusRTU m_rtu;
const int slave_addr = 1;   // Slave Address of Modbus Device
unsigned long prev_t = 0;
unsigned long interval = 5000;

void setup() {
  Serial.begin(9600);
  
  // configure serial to connect with modbus device
  // and set <m_rtu> serial to let <m_rtu> use that serial
  Serial1.begin(9600,SERIAL_8N1,RXmax,TXmax);
  m_rtu.initSerial(Serial1, DIRPIN);
  
  Serial.println("BEGIN");
  Serial.println();
}

void loop() {
  unsigned long curr_t = millis();
  if(curr_t-prev_t > interval || prev_t == 0) {
    prev_t = curr_t;
    m_rtu.sendReadHolding(slave_addr,0,1);        // addr = 0, length = 1 register (2 bytes)
    int data_recv = m_rtu.recv_int(slave_addr);   // return register's data (int)
    Serial.println("data : " + (String)data_recv);
  }
}
