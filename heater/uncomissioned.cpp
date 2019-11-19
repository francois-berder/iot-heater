#include "board.h"
#include "uncommissioned.h"
#include "Arduino.h"
#include "Ticker.h"
#include "webpages.h"
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define SLOW_BLINK_PERIOD     (500) /* In ms */

#define DNS_PORT              (53)

static Ticker leds_ticker;
static AsyncWebServer server(80);
static DNSServer dns_server;

static void toggle_leds()
{
    digitalWrite(LED1_PIN, !digitalRead(LED1_PIN));
    digitalWrite(LED2_PIN, !digitalRead(LED2_PIN));
}

void setup_uncommissioned(void)
{
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    leds_ticker.attach_ms(SLOW_BLINK_PERIOD, toggle_leds);

    /* Create Wifi AP */
    char ssid[16];
    sprintf(ssid, "heater-%08X", ESP.getChipId());
    char password[16];
    sprintf(password, "%08X", ESP.getChipId());
    IPAddress apIP(192, 168, 1, 1);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid, password);

    /* Start DNS server */
    dns_server.start(DNS_PORT, "www.heater.local", apIP);

    /* Spawn web server */
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send_P(200, "text/html", uncommissioned_index_html);
        }
    );
    server.begin();
}

void loop_uncommissioned(void)
{
    dns_server.processNextRequest();
}
