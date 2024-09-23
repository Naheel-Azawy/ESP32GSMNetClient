#ifndef NET_CLIENT_H_
#define NET_CLIENT_H_

#define TINY_GSM_MODEM_SIM7600 // SIMA7670 Compatible with SIM7600 AT instructions
#define TINY_GSM_DEBUG Serial
#include <TinyGsmClient.h>

#include <WiFi.h>

#define NET_ADD_SSL
#ifdef NET_ADD_SSL
#include <SSLClient.h>
#endif

#define NET_TASK_CORE            -1    // -1 to not pin to any core
#define NET_TASK_STACK_SIZE      20000 // bytes
#define NET_CONNECT_TIMEOUT      5000  // ms
#define WIFI_TIMEOUT             3000  // ms; not really effective
#define WIFI_DOUBLE_CHECK_PERIOD 10000 // ms

typedef enum {
    NET_WIFI_FIRST,
    NET_WIFI_ONLY,
    NET_GSM_ONLY
} NetMode;

typedef enum {
    NET_CON_NONE,
    NET_CON_WIFI,
    NET_CON_GSM
} NetConnection;

using OnNetChange = std::function<void(bool connected, NetConnection mode)>;

class NetClass {
public:
    NetMode       mode           = NET_WIFI_FIRST;
    NetConnection connection     = NET_CON_NONE;
    TinyGsm      *modem          = NULL;

    const char *wifi_ssid;
    const char *wifi_passwd;
    const char *gsm_apn;
    const char *gsm_user;
    const char *gsm_passwd;
    const char *gsm_pin;

    bool started        = false;
    bool wifi_connected = false;
    bool gsm_connected  = false;
    bool can_use_gsm    = true;

    OnNetChange       onchange               = NULL;
    unsigned long     last_connection_at     = 0; // used in NetClient
    int               connect_failed_count   = 0;

    /*
      - Option 1: using openssl
        + openssl s_client -showcerts -connect domain.com:443 </dev/null
      - Option 2: using web browser
        + Form browser, go to something like chrome://settings/certificates
        + Under "Authorities"
        + For this particular case, "org-Internet Security Research Group" and "ISRG Root X1"
          Check the certificate authority for your domain
        + Export
    */
    const char *ssl_ca_cert = NULL; // NULL means no SSL

    NetClass();
    void begin(NetMode mode,
               const char *wifi_ssid=NULL, const char *wifi_passwd=NULL,
               const char *gsm_apn="data",
               const char *gsm_user="", const char *gsm_passwd="",
               const char *gsm_pin="");
    void    start();
    void    end();
    bool    connected();
    bool    gsm_connect_again();
    void    gsm_task(bool loop=true);
    void    wifi_task(bool loop=true);
    void    loop();
    void    run_onchange();
};

extern NetClass Net;

// Client interface

class NetClient : public Client {
public:
    Client        *real_client   = NULL;
    Client        *real_client_2 = NULL; // if ssl is used, this is the inner client
    unsigned long  connection_at;        // used to stop when a newer connection is made
    NetConnection  client_connection;    // used to stop when mode is changed

    ~NetClient();
    NetClient();
    NetClient(WiFiClient c) : NetClient() {
        // Ignored. Just to compile WebSocketsServer at
        // new WEBSOCKETS_NETWORK_CLASS(_server->available());
    }

    IPAddress remoteIP() {
        Serial.println("remoteIP() not implemented because of TinyGSM");
        IPAddress ip(0, 0, 0, 0);
        return ip;
    }

    int     connect(IPAddress ip, uint16_t port);
    int     connect(const char *host, uint16_t port);
    int     connect(IPAddress ip, uint16_t port, int32_t timeout);
    int     connect(const char *host, uint16_t port, int32_t timeout);
    size_t  write(uint8_t b);
    size_t  write(const uint8_t *buf, size_t size);
    size_t  write(const char *buf);
    int     available();
    int     read();
    int     read(uint8_t *buf, size_t size);
    int     peek();
    void    flush();
    void    stop();
    uint8_t connected();
    operator bool() { return this->connected(); }
};

// utils

void http_get_req(const char *server, const char *resource="/");
void http_test();

#endif // NET_CLIENT_H_
