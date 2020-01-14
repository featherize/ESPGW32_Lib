// SDM120CT-MV
// Wiring   : http://www.etteam.com/productDIN/SDM120CT-MV/%E0%B8%81%E0%B8%B2%E0%B8%A3%E0%B8%95%E0%B9%88%E0%B8%AD%E0%B9%83%E0%B8%8A%E0%B9%89%E0%B8%87%E0%B8%B2%E0%B8%99_SMD120CT-MV.pdf
// Protocol : http://www.etteam.com/productDIN/SDM120CT-MV/SDM120CT%20PROTOCOL.pdf

#include "ESPGW32.h"

CprE_modbusRTU m_rtu;
const int slave_addr = 1;
unsigned long prev_t = 0;
unsigned long interval = 10000;

void setup() {
  Serial.begin(9600);

  // configure serial to connect with SDM120CT-MV
  Serial1.begin(2400,SERIAL_8N1,RXmax,TXmax);
  m_rtu.initSerial(Serial1, DIRPIN);

  Serial.println("BEGIN");
  Serial.println();
}

void loop() {
  unsigned long curr_t = millis();
  if(curr_t-prev_t > interval || prev_t == 0) {
    prev_t = curr_t;
    
    m_rtu.sendReadInput(slave_addr,0,2);        // read voltage
    float volt = m_rtu.recv_float(slave_addr);  // receive response
    Serial.println("Voltage                : " + (String)volt + " Volts");

    m_rtu.sendReadInput(slave_addr,6,2);        // read current
    float current = m_rtu.recv_float(slave_addr);
    Serial.println("Current                : " + (String)current + " Amps");
    m_rtu.sendReadInput(slave_addr,12,2);       // read active power
    float actPower = m_rtu.recv_float(slave_addr);
    Serial.println("Active Power           : " + (String)actPower + " Watts");
    m_rtu.sendReadInput(slave_addr,18,2);       // read apparent power
    float appPower = m_rtu.recv_float(slave_addr);
    Serial.println("Apparent Power         : " + (String)appPower + " VoltAmps");
    m_rtu.sendReadInput(slave_addr,24,2);       // read reactive power
    float reactPower = m_rtu.recv_float(slave_addr);
    Serial.println("Reactive Power         : " + (String)reactPower + " VAr");
    m_rtu.sendReadInput(slave_addr,30,2);       // read power factor
    float pfactor = m_rtu.recv_float(slave_addr);
    Serial.println("Power Factor           : " + (String)pfactor);
    m_rtu.sendReadInput(slave_addr,36,2);       // read phase angle
    float phase = m_rtu.recv_float(slave_addr);
    Serial.println("Phase Angle            : " + (String)phase + " Degrees");
    m_rtu.sendReadInput(slave_addr,70,2);       // read frequency
    float freq = m_rtu.recv_float(slave_addr);
    Serial.println("Frequency              : " + (String)freq + " Hz");
    m_rtu.sendReadInput(slave_addr,72,2);       // read import active energy
    float imAct = m_rtu.recv_float(slave_addr);
    Serial.println("Import Active Energy   : " + (String)imAct + " kWh");
    m_rtu.sendReadInput(slave_addr,74,2);       // read export active energy
    float exAct = m_rtu.recv_float(slave_addr);
    Serial.println("Export Active Energy   : " + (String)exAct + " kWh");
    m_rtu.sendReadInput(slave_addr,76,2);       // read import reactive energy
    float imReact = m_rtu.recv_float(slave_addr);
    Serial.println("Import Reactive Energy : " + (String)imReact + " kVArh");
    m_rtu.sendReadInput(slave_addr,78,2);       // read export reactive energy
    float exReact = m_rtu.recv_float(slave_addr);
    Serial.println("Export Reactive Energy : " + (String)exReact + " kVArh");
    m_rtu.sendReadInput(slave_addr,342,2);      // read total active energy
    float totalAct = m_rtu.recv_float(slave_addr);
    Serial.println("Total Active Energy    : " + (String)totalAct + " kWh");
    m_rtu.sendReadInput(slave_addr,344,2);      // read total reactive energy
    float totalReact = m_rtu.recv_float(slave_addr);
    Serial.println("Total Reactive Energy  : " + (String)totalReact + " kVArh");
  }
}
