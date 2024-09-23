#include "net.hpp"
#include <HttpClient.h>

void http_get_req(const char *server, const char *resource) {
    long start = millis();
    
    Serial.print("Performing HTTP");
    if (Net.ssl_ca_cert != NULL) Serial.print("S");
    Serial.print(" GET request... ");
    if (!Net.connected()) {
        Serial.println("Net is not connected");
        return;
    }
    Serial.println();

    Client *client = new NetClient();
    HttpClient *http = new HttpClient(*client, server,
                                      Net.ssl_ca_cert != NULL ? 443 : 80);

    http->setHttpResponseTimeout(10000);

    int err = http->get(resource);
    if (err != 0) {
        Serial.println(F("failed to connect"));
        return;
    }

    int status = http->responseStatusCode();
    Serial.print(F("Response status code: "));
    Serial.println(status);
    if (!status) {
        return;
    }

    Serial.println(F("Response Headers:"));
    while (http->headerAvailable()) {
        String headerName  = http->readHeaderName();
        String headerValue = http->readHeaderValue();
        Serial.println("    " + headerName + " : " + headerValue);
    }

    int length = http->contentLength();
    if (length >= 0) {
        Serial.print(F("Content length = "));
        Serial.println(length);
    }

    String body = http->responseBody();
    Serial.println(F("Response:"));
    Serial.println(body);

    Serial.print(F("Body length = "));
    Serial.println(body.length());

    http->stop();
    Serial.println(F("Server disconnected"));
    Serial.printf("Took %d millis\n\r", millis() - start);

    delete http;
    delete client;
}

void http_test() {
    http_get_req("naheel.xyz", "/ipgeo");
}
