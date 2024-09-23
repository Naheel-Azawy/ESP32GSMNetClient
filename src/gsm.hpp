#ifndef NET_CLIENT_GSM_H_
#define NET_CLIENT_GSM_H_

#define TINY_GSM_MODEM_SIM7600 // SIMA7670 Compatible with SIM7600 AT instructions
#define TINY_GSM_DEBUG Serial
#include <TinyGsmClient.h>

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

// See all AT commands, if wanted
//#define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
#endif

#define GSM_UART_BAUD    115200
#define GSM_PIN_TX       26
#define GSM_PIN_RX       27
#define GSM_PWR_PIN      4
#define GSM_RESET        5

TinyGsm global_modem(SerialAT);

#define GSM_TIMEOUT_CHECK(step)                 \
    {                                           \
        long diff = millis() - start;           \
        if (diff >= retry_timeout) {            \
            DBG("GSM timeout at " step);        \
            break;                              \
        }                                       \
    }

#define GSM_OK_CHECK(step)                          \
    {                                               \
        if (!ok) {                                  \
            DBG("GSM setup is not ok after " step); \
            return false;                           \
        }                                           \
    }

bool gsm_connect(TinyGsm    *modem,
                 const char *gsm_pin,
                 const char *apn,
                 const char *gprs_user,
                 const char *gprs_passwd);

bool gsm_start(TinyGsm    *modem,
               const char *gsm_pin="",
               const char *apn="data",
               const char *gprs_user="",
               const char *gprs_passwd="") {
    bool ok;
    long start;
    int retry_timeout;

    // setup =================

    start = millis();
    retry_timeout = 20000;
    int attempts_max   = 2;
    int setup_attempts = 0;
    ok = false;
    while (!ok) {
        GSM_TIMEOUT_CHECK("setup");
        delay(10);

        // A7670 Reset
        pinMode(GSM_RESET, OUTPUT);
        digitalWrite(GSM_RESET, LOW);
        delay(100);
        digitalWrite(GSM_RESET, HIGH);
        delay(3000);
        digitalWrite(GSM_RESET, LOW);

        pinMode(GSM_PWR_PIN, OUTPUT);
        digitalWrite(GSM_PWR_PIN, LOW);
        delay(100);
        digitalWrite(GSM_PWR_PIN, HIGH);
        delay(1000);
        digitalWrite(GSM_PWR_PIN, LOW);

        DBG("GSM Wait...");

        delay(3000);

        SerialAT.begin(GSM_UART_BAUD, SERIAL_8N1, GSM_PIN_RX, GSM_PIN_TX);

        // Restart takes quite some time
        // To skip it, call init() instead of restart()
        DBG("Initializing modem...");
        if (!modem->init()) {
            ok = false;
            if (++setup_attempts > attempts_max) {
                DBG("Failed to init modem, closing...");
                return false;
            } else {
                DBG("Failed to init modem, retrying...");
                continue;
            }
        }

        /*
          2 Automatic
          13 GSM Only
          14 WCDMA Only
          38 LTE Only
        */
        bool result = modem->setNetworkMode(38);
        if (!result) {
            DBG("setNetworkMode() failed, skipping...");
            // it's ok if this fails
        } else {
            DBG("setNetworkMode() ok");
        }

        ok = true;
    }
    GSM_OK_CHECK("setup");

    // init ==================

    start = millis();
    retry_timeout = 20000;
    ok = false;
    while (!ok) {
        GSM_TIMEOUT_CHECK("init");

        // Restart takes quite some time
        // To skip it, call init() instead of restart()
        DBG("Initializing modem...");
        if (!modem->init()) {
            DBG("Failed to init modem, retrying...");
            ok = false;
            continue;
        }

        String name = modem->getModemName();
        DBG("Modem Name:", name);

        String modemInfo = modem->getModemInfo();
        DBG("Modem Info:", modemInfo);

        // Unlock your SIM card with a PIN if needed
        if (gsm_pin && modem->getSimStatus() != 3) {
            modem->simUnlock(gsm_pin);
        }

        ok = true;
    }
    GSM_OK_CHECK("init");

    // net connect ==================

    start = millis();
    retry_timeout = 20000;
    ok = false;
    while (!ok) {
        GSM_TIMEOUT_CHECK("net connect");

        DBG("Waiting for network...");
        if (!modem->waitForNetwork(retry_timeout / 2)) {
            DBG("fail");
            delay(1000);
            ok = false;
            continue;
        }
        DBG("success");

        if (modem->isNetworkConnected()) {
            DBG("Network connected");
        } else {
            DBG("Network NOT connected");
        }

        ok = true;
    }
    GSM_OK_CHECK("net connect");

    return gsm_connect(modem,
                       gsm_pin, apn,
                       gprs_user, gprs_passwd);
}

bool gsm_connect(TinyGsm    *modem,
                 const char *gsm_pin="",
                 const char *apn="data",
                 const char *gprs_user="",
                 const char *gprs_passwd="") {
    bool ok;
    long start;
    int retry_timeout;

    // connect ==================

    start = millis();
    retry_timeout = 10000;
    ok = false;
    while (!ok) {
        GSM_TIMEOUT_CHECK("connect");

        // GPRS connection parameters are usually set after network registration
        DBG(F("Connecting to "));
        DBG(apn);
        if (!modem->gprsConnect(apn, gprs_user, gprs_passwd)) {
            DBG(" fail");
            delay(1000);
            ok = false;
            continue;
        }
        DBG(" success");

        if (modem->isGprsConnected()) {
            DBG("GPRS connected");
        } else {
            DBG("GPRS NOT connected");
        }

        ok = true;
    }
    GSM_OK_CHECK("connect");

    bool res = modem->isGprsConnected();
    DBG("GPRS status:", res ? "connected" : "not connected");

    String ccid = modem->getSimCCID();
    DBG("CCID:", ccid);

    String imei = modem->getIMEI();
    DBG("IMEI:", imei);

    String imsi = modem->getIMSI();
    DBG("IMSI:", imsi);

    String cop = modem->getOperator();
    DBG("Operator:", cop);

    IPAddress local = modem->localIP();
    DBG("Local IP:", local);

    int csq = modem->getSignalQuality();
    DBG("Signal quality:", csq);

    return true;
}

void gsm_end(TinyGsm *modem) {
    modem->gprsDisconnect();
    DBG(F("GPRS disconnected"));
}

#endif // NET_CLIENT_GSM_H_
