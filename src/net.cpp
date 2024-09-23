#include <Arduino.h>

#include "net.hpp"
#include "wifi.hpp"
#include "gsm.hpp"

NetClass Net;

NetClass::NetClass() {
    this->modem = &global_modem;
}

void NetClass::begin(NetMode mode,
                     const char *wifi_ssid, const char *wifi_passwd,
                     const char *gsm_apn,
                     const char *gsm_user, const char *gsm_passwd,
                     const char *gsm_pin) {
    this->mode = mode;

    this->wifi_ssid   = wifi_ssid;
    this->wifi_passwd = wifi_passwd;
    this->gsm_apn     = gsm_apn;
    this->gsm_user    = gsm_user;
    this->gsm_passwd  = gsm_passwd;
    this->gsm_pin     = gsm_pin;

    // disable wifi if no ssid is provided
    if (this->wifi_ssid == NULL)
        this->mode = NET_GSM_ONLY;
}

void NetClass::start() {
    if (this->started) {
        DBG("Calling start() while already started, ignored.");
        return;
    }

    if (this->mode != NET_WIFI_ONLY) {
        const bool once = true;
        for (;;) {
            this->gsm_connected =
                gsm_start(this->modem, this->gsm_pin,
                          this->gsm_apn,
                          this->gsm_user, this->gsm_passwd);

            if (this->gsm_connected || once) {
                break;
            } else {
                Serial.println("GSM is not ok, retrying...");
                delay(500);
            }
        }
        //this->gsm_task(false);
        /*xTaskCreate([](void *arg) {
            Serial.println("Net: GSM task starting");
            auto n = (NetClass *) arg;
            n->gsm_task();
            Serial.println("Net: GSM task end");
            vTaskDelete(NULL);
            }, "Net: GSM task", 10000, this, 1, NULL);*/
    }

    if (this->mode != NET_GSM_ONLY) {
        this->wifi_connected =
            wifi_start(this->wifi_ssid, this->wifi_passwd,
                       WIFI_TIMEOUT);
        xTaskCreate([](void *arg) {
            Serial.println("Net: WiFi task starting");
            auto n = (NetClass *) arg;
            n->wifi_task();
            Serial.println("Net: WiFi task end");
            vTaskDelete(NULL);
        }, "Net: WiFi task", 10000, this, 1, NULL);
    }

    this->started = true;

    this->loop();
}

void NetClass::run_onchange() {
    if (this->onchange != NULL) {
        this->onchange(this->connected(), this->connection);
    }
}

void NetClass::end() {
    switch (this->connection) {
    case NET_CON_GSM:  gsm_end(this->modem); break;
    case NET_CON_WIFI: wifi_end();           break;
    }
    this->connection = NET_CON_NONE;
    this->run_onchange();
}

bool NetClass::gsm_connect_again() {
    if (this->mode == NET_WIFI_ONLY) {
        return false;
    }

    this->gsm_connected = gsm_connect(this->modem, this->gsm_pin,
                                      this->gsm_apn,
                                      this->gsm_user, this->gsm_passwd);
    if (this->gsm_connected) {
        this->last_connection_at = millis();
        Serial.println("GSM connected!");
    }
    this->loop();
    return this->gsm_connected;
}

void NetClass::gsm_task(bool loop) {
    if (this->mode == NET_WIFI_ONLY) {
        return;
    }

    do {
        if (this->modem && this->modem->isNetworkConnected()) {
            delay(1000);
            continue;
        }
        this->gsm_connect_again();
    } while (loop);
}

void NetClass::wifi_task(bool loop) {
    if (this->mode == NET_GSM_ONLY) {
        return;
    }

    do {
        if (WiFi.status() == WL_CONNECTED) {
            delay(1000);
            continue;
        }
        this->wifi_connected =
            wifi_start(this->wifi_ssid, this->wifi_passwd,
                       WIFI_TIMEOUT, true);
        if (this->wifi_connected) {
            this->last_connection_at = millis();
            Serial.println("WiFi connected");
        }
        this->loop();
    } while (loop);
}

void NetClass::loop() {
    NetConnection prev = this->connection;
    if (this->wifi_connected) {
        this->connection = NET_CON_WIFI;
    } else if (this->gsm_connected) {
        this->connection = NET_CON_GSM;
    } else {
        this->connection = NET_CON_NONE;
    }

    if (prev != this->connection) {
        this->run_onchange();
    }
}

bool NetClass::connected() {
    return this->connection != NET_CON_NONE &&
        ((WiFi.status() == WL_CONNECTED) ||
         (this->modem && this->modem->isNetworkConnected()));
}

// Client interface

NetClient::NetClient() {
    const bool prefer_secure = true;
    this->connection_at = Net.last_connection_at;
    this->client_connection = Net.connection;

    Client *c         = NULL;
    WiFiClient *wific = NULL;
    switch (this->client_connection) {
    case NET_CON_WIFI:
        wific = new WiFiClient();
        wific->setTimeout(WIFI_TIMEOUT / 1000);
        c = wific;
        DBG("NetClient::NetClient(): CREATED WiFiClient");
        break;
    case NET_CON_GSM:
        if (Net.modem == NULL) {
            DBG("NetClient::NetClient(): ERROR: modem is NULL");
            c = NULL;
        } else {
            c = new TinyGsmClient(*Net.modem);
            DBG("NetClient::NetClient(): CREATED TinyGsmClient");
        }
        break;
    case NET_CON_NONE:
        DBG("NetClient::NetClient(): ERROR: Net not connected");
        c = NULL;
        break;
    default:
        DBG("NetClient::NetClient(): ERROR: connection state is unknown");
        c = NULL;
    }

#ifdef NET_ADD_SSL
    if (prefer_secure && c != NULL && Net.ssl_ca_cert != NULL) {
        SSLClient *c_ssl = new SSLClient(c);
        c_ssl->setCACert(Net.ssl_ca_cert);
        this->real_client   = c_ssl;
        this->real_client_2 = c;
    } else {
        this->real_client   = c;
        this->real_client_2 = NULL;
    }
#else
    this->real_client   = c;
    this->real_client_2 = NULL;
#endif
}

// TODO: validate this fixes the memory leak
NetClient::~NetClient() {
    if (this->real_client != NULL) {
        delete this->real_client;
    }
    if (this->real_client_2 != NULL) {
        delete this->real_client_2;
    }
}

#define NET_CALL_PRINT(call, retval)                            \
    {                                                           \
        const char *con;                                        \
        switch (this->client_connection) {                      \
        case NET_CON_GSM:  con = "GSM";  break;                 \
        case NET_CON_WIFI: con = "WIFI"; break;                 \
        case NET_CON_NONE: con = "NONE"; break;                 \
        default:           con = "?";    break;                 \
        }                                                       \
        Serial.printf("!![%u] NET_CALL (%s): `%s' = %d\n\r",    \
                      millis(), con, #call, retval);            \
    }

#define NET_CALL_BASE(call, ret, retret, print)             \
    {                                                       \
        int retval;                                         \
        if (Net.connection != this->client_connection    || \
            this->client_connection == NET_CON_NONE      || \
            Net.last_connection_at > this->connection_at || \
            this->real_client == NULL                    || \
            (this->client_connection == NET_CON_GSM &&      \
             !Net.can_use_gsm)) {                           \
            ret 0;                                          \
        } else {                                            \
            ret this->real_client->call;                    \
        }                                                   \
        if (print) NET_CALL_PRINT(call, retval);            \
        retret;                                             \
    }

#define NET_CALL(call, print)      NET_CALL_BASE(call, retval =, return retval, print)
#define NET_CALL_VOID(call, print) NET_CALL_BASE(call, (void), (void) 0, print)

#define NET_CALL_CONNECT(call, print)                           \
    {                                                           \
        NET_CALL_BASE(call, retval =, {                         \
                if (retval == 0) {                              \
                    /* because sometimes Net.working() */       \
                    /* stay true with WiFi*/                    \
                    if (++Net.connect_failed_count >= 2) {      \
                        Serial.println("NetClient::"            \
                                       "connect() failed ");    \
                    }                                           \
                } else {                                        \
                    Net.connect_failed_count = 0;               \
                }                                               \
                return retval;                                  \
            }, print);                                          \
    }

int     NetClient::connect(IPAddress ip, uint16_t port)                      NET_CALL_CONNECT(connect(ip,   port),               false);
int     NetClient::connect(const char *host, uint16_t port)                  NET_CALL_CONNECT(connect(host, port),               false);
int     NetClient::connect(IPAddress ip, uint16_t port, int32_t timeout)     NET_CALL_CONNECT(connect(ip,   port),               false);
int     NetClient::connect(const char *host, uint16_t port, int32_t timeout) NET_CALL_CONNECT(connect(host, port),               false);
size_t  NetClient::write(uint8_t b)                                          NET_CALL(write(b),                                  false);
size_t  NetClient::write(const uint8_t *buf, size_t size)                    NET_CALL(write(buf, size),                          false);
size_t  NetClient::write(const char *buf)                                    NET_CALL(write((const uint8_t *) buf, strlen(buf)), false);
int     NetClient::available()                                               NET_CALL(available(),                               false);
int     NetClient::read()                                                    NET_CALL(read(),                                    false);
int     NetClient::read(uint8_t *buf, size_t size)                           NET_CALL(read(buf, size),                           false);
int     NetClient::peek()                                                    NET_CALL(peek(),                                    false);
void    NetClient::flush()                                                   NET_CALL_VOID(flush(),                              false);
void    NetClient::stop()                                                    NET_CALL_VOID(stop(); delete this->real_client,     false);
uint8_t NetClient::connected()                                               NET_CALL(connected(),                               false);
