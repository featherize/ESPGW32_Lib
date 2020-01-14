#include "ESPGW32.h"

CprE_modbusRTU m_rtu;
const int slave_addr = 1;   // Slave Address of Modbus Device

void setup() {
  Serial.begin(9600);
  
  // configure serial to connect with modbus device
  // and set <m_rtu> serial to let <m_rtu> use that serial
  Serial1.begin(9600,SERIAL_8N1,RXmax,TXmax);
  m_rtu.initSerial(Serial1, DIRPIN);
  
  Serial.println("BEGIN");
  Serial.println();

  /* packet with CRC16 */
  uint8_t packet[8] = {0x01,0x03,0x00,0x10,0x00,0x01,0x85,0xcf};
  m_rtu.sendpacket(packet, 8);
  /* packet without CRC16 (Auto gen crc) */
//  uint8_t packet[6] = {0x01,0x03,0x00,0x10,0x00,0x01};
//  m_rtu.sendpacket(packet, 6, true);
  
  m_rtu.recv(slave_addr);
  Serial.print("response = ");
  for(int i=0; i<m_rtu.buf_length(); i++) {
    Serial.print(m_rtu.buf[i],HEX);
    Serial.print(" ");
  }
  Serial.println();
  if(m_rtu.getError()) {
    Serial.print("ERROR! ");
    Serial.println(m_rtu.errorReport());
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
