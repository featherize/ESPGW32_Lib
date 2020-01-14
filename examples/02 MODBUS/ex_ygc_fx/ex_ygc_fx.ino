// YGC-FX wind direction
// Wiring: red->12V, black->A, yellow->B, green->GND

#include "ESPGW32.h"

CprE_modbusRTU m_rtu;
const int slave_addr = 5;   // Slave Address of Modbus Device
unsigned long prev_t = 0;
unsigned long interval = 1000;

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600,SERIAL_8N1,RXmax,TXmax);
  m_rtu.initSerial(Serial1, DIRPIN);
  
  Serial.println("BEGIN");
  m_rtu.sendReadHolding(0,1,1);                 // read address (boardcast)
  int resp_addr = m_rtu.recv_int(0);
  Serial.print("Query address = ");
  Serial.println(resp_addr);
  m_rtu.sendReadHolding(slave_addr,16,1);       // read baudrate
  Serial.print("Query baudrate = ");
  Serial.println(m_rtu.recv_int(slave_addr));
  Serial.println("----------------------------------------");
}

void loop() {
  unsigned long curr_t = millis();
  if(curr_t-prev_t > interval || prev_t == 0) {
    prev_t = curr_t;
    m_rtu.sendReadHolding(slave_addr,0,1);      // read wind direction
    int dir = m_rtu.recv_int(slave_addr);
    Serial.println("Wind direction = " + (String)dir + "\u00B0 (clockwise from south)");
  }
}
