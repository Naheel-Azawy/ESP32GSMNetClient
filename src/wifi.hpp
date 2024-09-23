#ifndef NET_CLIENT_WIFI_H_
#define NET_CLIENT_WIFI_H_

#include <WiFi.h>

#if defined(ESP32)
#define WIFI_START(ssid, passwd) WiFi.begin(ssid, passwd)
#define WIFI_CONNECTED() WiFi.isConnected()
#elif defined(ESP8266)
#define WIFI_START(ssid, passwd) WiFiMulti.addAP(ssid, passwd)
#define WIFI_CONNECTED() (WiFiMulti.run() == WL_CONNECTED)
#endif

bool wifi_start(const char *ssid, const char *passwd,
                int retry_timout=5000, bool silent=false) {
    if (!silent) {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
    }

    long start = millis();

    WIFI_START(ssid, passwd);
    while (!WIFI_CONNECTED()) {
        if (millis() - start >= retry_timout) {
            return false;
        }
        if (!silent) Serial.print(".");
        delay(500);
    }

    if (!silent) {
        Serial.print("Connected to ");
        Serial.println(ssid);
    }

    return true;
}

void wifi_end() {
    WiFi.disconnect();
    DBG(F("WiFi disconnected"));
}

#endif // NET_CLIENT_WIFI_H_
