#include <Arduino.h>
#include <NetClient.h>
#include <WebSockets.h>

#define WIFI_CRED "quickbrownfox", "lazy@dog"
#define GSM_APN   "data"

void setup() {
    Serial.begin(115200);

    Net.begin(NET_WIFI_FIRST, WIFI_CRED, GSM_APN);
    Net.start();

    http_get_req("naheel.xyz", "/ipgeo");

    Net.end();
}

void loop() {
    delay(1000);
}
