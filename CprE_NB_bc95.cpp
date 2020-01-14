/*
CprE_NB_BC95 v1.0
Author: CprE IoT
Create Date: 1 JUL 2018
Modified: 1 JUL 2018
Released for Maker and Developer
Modified : 2018.JUL.1
Re-coding to compatible with IoTtweet.com
*/

#include "CprE_NB_bc95.h"

bool CprE_NB_bc95::reboot() {
  MODEM_SERIAL -> println(F("AT+NRB"));
}

void CprE_NB_bc95::init(Stream &serial) {
  MODEM_SERIAL = &serial;
}

String CprE_NB_bc95::getIMEI()
{
  MODEM_SERIAL->println(F("AT+CGSN=1")); // Request Product Serial Number
  char waitForCGSN[] = "+CGSN:";
  String re_str;
  re_str = expect_rx_str(1000, waitForCGSN, 6);

  if (re_str != "") {
    return re_str;
  }
  return "";
}

String CprE_NB_bc95::getIMSI()
{
  MODEM_SERIAL->println(F("AT+CIMI")); // Request International Mobile Subscriber Identity
  char waitForMISI[] = "\r\n";
  String re_str;
  re_str = expect_rx_str(1000, waitForMISI, 2);

  if (re_str != "") {
    return re_str;
  }
  return "";
}

String CprE_NB_bc95::expect_rx_str( unsigned long period, char exp_str[], int len_check) {
  unsigned long cur_t = millis();
  unsigned long start_t = millis();
  bool str_found = 0;
  bool time_out = 0;
  bool loop_out = 0;
  int i = 0;
  int found_index = 0, end_index = 0;
  int modem_said_len = 0;
  char c;
  char modem_said[MODEM_RESP];
  String re_str;
  char str[BUF_MAX_SIZE];
  char *x;

  while ( !loop_out ) {
    if (MODEM_SERIAL->available()) {
      c = MODEM_SERIAL->read();
      modem_said[i++] = c;
    }
    else {
    }
    cur_t = millis();
    if ( cur_t - start_t > period ) {
      time_out = true;
      start_t = cur_t;
      loop_out = true;
    }
  }//while
  modem_said[i] = '\0';
  end_index = i;
  x = strstr(modem_said, exp_str) ;
  found_index = x ? x - modem_said : -1;
  if ( found_index >= 0  ) {
    i = 0;
    while ( modem_said[found_index + i + len_check] != 0x0D | i == 0) {
      str[i] = modem_said[found_index + i + len_check];
      re_str += String(str[i]);
      i++;
    }
    str[i] = '\0';
    return re_str;
  }
  return "";
}

bool CprE_NB_bc95::initModem() {
  // Serial.println(F("######### CprE_NB_bc95 Library based on True_NB_BC95 ##########"));
  // Serial.println( "initial Modem to connect NB-IoT Network" );
  MODEM_SERIAL->println(F("AT+NRB"));
  delay(5000);
  char waitForReboot[] = "REBOOTING";
  if ( expect_rx_str(2000, waitForReboot, 9) != "" ) {
    // Serial.println("Reboot done Connecting to Network");
  }
  MODEM_SERIAL->println(F("AT+CFUN=1"));
  delay(2000);
  MODEM_SERIAL->println(F("AT"));
  delay(1000);
}

bool CprE_NB_bc95::register_network() {
  bool regist = 0;
  delay(2000);
  MODEM_SERIAL->println(F("AT+CGATT=1")); // Activate the network.
  delay(3000);
  /* Query whether network is activated, +CGATT:1 means activated successfully,
     sometimes customers need to wait for 30s.
  */
  MODEM_SERIAL->println(F("AT+CGATT?")); // Query whether network is activated
  delay(1000);
  char waitForCGATT[] = "+CGATT:1"; // +CGATT:1 means activated successfully
  if (expect_rx_str(1000, waitForCGATT, 8 ) != "" ) {
    Serial.println("register network Done!");
    return true;
  }
  else {
    Serial.println("register network Fail!");
    return false;
  }
}

String CprE_NB_bc95::check_ipaddr() {
  MODEM_SERIAL->println(F("AT+CGPADDR=0")); // Show PDP Addresses
  delay(1000);
  char waitForCGPADDR[] = "+CGPADDR:0,";
  String re_str;
  re_str = expect_rx_str(1000, waitForCGPADDR, 11 );
  if ( re_str != "" ) {
    return re_str;
  }
  return "";
}

int CprE_NB_bc95::check_modem_signal() {
  char resp_result[BUF_MAX_SIZE];
  int index = 0;
  int ssi;
  char ssi_str[3];
  MODEM_SERIAL->println("AT+CSQ");
  char waitForCSQ[] = "+CSQ:";
  String re_str;
  re_str = expect_rx_str( 1000, waitForCSQ, 5);
  if ( re_str != "" ) {
    ssi_str[0] = re_str[0];
    // check the next char is not "," It is not single digit
    if ( re_str[1] != 0x2c) {
      //Serial.println( resp_result[index+2]);
      ssi_str[1] = re_str[1];
      ssi_str[2] = '\0';
      ssi = atoi(ssi_str);
      ssi = -1 * (113 - ssi * 2);
      return ssi;
    }
    // it is single digit
    ssi_str[1] = '\0';
    ssi = atoi(ssi_str);
    ssi = -1 * (113 - ssi * 2);
    return ssi;
  }
  else {
    return -200;
  }

}

bool CprE_NB_bc95::create_UDP_socket(int port, char sock_num[]) {

  MODEM_SERIAL->print(F("AT+NSOCR=DGRAM,17,")); // supported value is DGRAM, UDP is 17
  MODEM_SERIAL->print( port );
  MODEM_SERIAL->println(F(",1")); // Set to 1 if incoming messages should be received
  delay(2000);
  if ( expect_rx_str(1000, sock_num, 1) != "" ) {
    return true;
  }
  return false;
}

bool CprE_NB_bc95::sendUDPstr(String ip, String port, String data) {

  int str_len = data.length();
  char buffer[str_len+2];
  data.toCharArray(buffer, str_len+1);

  /* Start AT command */
  MODEM_SERIAL->print(F("AT+NSOST=0"));
  MODEM_SERIAL->print(F(","));
  MODEM_SERIAL->print(ip);
  MODEM_SERIAL->print(F(","));
  MODEM_SERIAL->print(port);
  MODEM_SERIAL->print(F(","));
  MODEM_SERIAL->print(String(str_len));
  MODEM_SERIAL->print(F(","));
  /*
  Serial.print("AT+NSOST=0");
  Serial.print(",");
  Serial.print(ip);
  Serial.print(",");
  Serial.print(port);
  Serial.print(",");
  Serial.print(String(str_len));
  Serial.println(",");
  /*

  /* Fetch print data in hex format */
  char *h_buf;
  h_buf = buffer;
  char fetch[3] = "";
  bool chk = false;
  int i=0;

  while(*h_buf)
  {
    chk = itoa((int)*h_buf,fetch,16);
    if(chk){
      MODEM_SERIAL->print(fetch);
    }
    h_buf++;
  }
  MODEM_SERIAL->print("\r\n");
}

String CprE_NB_bc95::WriteDashboardIoTtweet(String userid, String key, float slot0, float slot1, float slot2, float slot3, String tw, String twpb){

  _userid = userid;
  _key = key;
  _slot0 = slot0;
  _slot1 = slot1;
  _slot2 = slot2;
  _slot3 = slot3;
  _tw = tw;
  _twpb = twpb;

//  Serial.println("------ Send to Cloud.IoTtweet -------");
             _packet = _userid;
             _packet += ":";
             _packet += _key;
             _packet += ":";
             _packet += String(_slot0);
             _packet += ":";
             _packet += String(_slot1);
             _packet += ":";
             _packet += String(_slot2);
             _packet += ":";
             _packet += String(_slot3);
             _packet += ":";
             _packet += _tw;
             _packet += ":";
             _packet += _twpb;

  Serial.println("packet sent : " + String(_packet));
//  Serial.println("--------------------------------------");

  Serial.println("Sending...");
  Serial.println("");

  sendUDPstr(IoTtweetNBIoT_HOST, IoTtweetNBIoT_PORT, _packet);
  return "OK";

}
