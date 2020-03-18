#include "board.h"
#include "uncommissioned.h"
#include "settings.h"
#include "Arduino.h"
#include "Ticker.h"
#include "webpages.h"
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define BUTTON_PRESS_TIMEOUT  (10000)   /* Timeout in milliseconds */
#define JOIN_NETWORK_TIMEOUT  (10000)   /* Timeout in milliseconds */

#define SLOW_BLINK_PERIOD     (500) /* In ms */

#define DNS_PORT              (53)

static Ticker leds_ticker;
static AsyncWebServer server(80);
static DNSServer dns_server;

static bool button_pressed;
static unsigned long button_pressed_start;

static bool joining_wifi_network;
static String device_name;
static String wifi_ssid;
static String wifi_password;
static unsigned long joining_wifi_network_start;

static void toggle_leds()
{
    digitalWrite(LED1_PIN, !digitalRead(LED1_PIN));
    digitalWrite(LED2_PIN, !digitalRead(LED2_PIN));
}

void setup_uncommissioned(void)
{
    pinMode(LEFT_OUTPUT_PIN, OUTPUT);
    pinMode(RIGHT_OUTPUT_PIN, OUTPUT);
    digitalWrite(LEFT_OUTPUT_PIN, 1);
    digitalWrite(RIGHT_OUTPUT_PIN, 0);
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    leds_ticker.attach_ms(SLOW_BLINK_PERIOD, toggle_leds);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    /* Create Wifi AP */
    byte mac[6];
    WiFi.macAddress(mac);
    char ssid[32];
    sprintf(ssid, "heater-%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    char password[32];
    sprintf(password, "%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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

    server.on("/register", HTTP_POST, [] (AsyncWebServerRequest *request) {
      if (request->hasParam("name", true)
      &&  request->hasParam("ssid", true)
      &&  request->hasParam("password", true)) {
        /* Attempt to join SSID/Password */
        if (!joining_wifi_network) {
            device_name = request->getParam("name", true)->value();
            wifi_ssid = request->getParam("ssid", true)->value();
            wifi_password = request->getParam("password", true)->value();
            if (device_name.length() < 32 && wifi_ssid.length() < 64 && wifi_password.length() < 64) {
                joining_wifi_network_start = millis();
                joining_wifi_network = true;
            } else {
                request->send_P(200, "text/html", uncommissioned_index_html);
            }
        }
    } else {
        request->send_P(200, "text/html", uncommissioned_index_html);
      }
    }
    );
    server.begin();
}

void loop_uncommissioned(void)
{
    /* Clear configuration if button is pressed for a while */
    if (digitalRead(BUTTON_PIN) == 0) {
        if (!button_pressed) {
            button_pressed = true;
            button_pressed_start = millis();
        } else if (millis() - button_pressed_start >= BUTTON_PRESS_TIMEOUT) {
            settings_erase();
            ESP.restart();
        }
    } else {
        button_pressed = false;
    }

    if (joining_wifi_network) {
        server.end();
        dns_server.stop();
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifi_ssid, wifi_password);
        while (WiFi.status() != WL_CONNECTED && (millis() - joining_wifi_network_start) < JOIN_NETWORK_TIMEOUT) {
            delay(500);
        }
        if (WiFi.status() == WL_CONNECTED) {
            settings_create(device_name, wifi_ssid, wifi_password);
        }
        ESP.restart();
    }
    
    dns_server.processNextRequest();
}
