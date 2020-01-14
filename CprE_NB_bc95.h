#ifndef CPRE_NB_BC95
#define CPRE_NB_BC95

#include <Stream.h>
#include <Arduino.h>

#define COAP_HEADER_SIZE 4
#define COAP_OPTION_HEADER_SIZE 1
#define COAP_PAYLOAD_MARKER 0xFF
#define MAX_OPTION_NUM 10
#define BUF_MAX_SIZE 64
#define COAP_DEFAULT_PORT 5683


#define COAP_VER 1
#define COAP_TYPE_CON  0
#define COAP_TYPE_NONCON  1
#define COAP_TYPE_ACK  2
#define COAP_TYPE_RESET  3
#define COAP_GET  1
#define COAP_POST  2
#define COAP_PUT  3
#define COAP_DELETE  4

#define MODEM_RESP 128

/* Dashboard Partner parameter : IoTtweet.com */
#define IoTtweetNBIoT_HOST "35.185.177.33"    // - New Cloud IoTtweet server IP
#define IoTtweetNBIoT_PORT "5683"             // - Default udp port

class CprE_NB_bc95 {
  public:
    void init(Stream &seial);
    bool reboot();
    String getIMSI();
    String getIMEI();
    String expect_rx_str( unsigned long period, char exp_str[], int len_check);
    bool initModem();
    bool register_network();
    String check_ipaddr();
    int check_modem_signal();
    bool create_UDP_socket(int port, char sock_num[]);
    bool sendUDPstr(String ip, String port, String data);
    String WriteDashboardIoTtweet(String userid, String key, float slot0, float slot1, float slot2, float slot3, String tw, String twpb);

  private:
    Stream* MODEM_SERIAL;
    String _packet, _userid, _key, _tw, _twpb;
    float _slot0, _slot1, _slot2, _slot3;

};

#endif
