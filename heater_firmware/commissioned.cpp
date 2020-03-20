#include "board.h"
#include "commissioned.h"
#include "settings.h"
#include "webpages.h"
#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define BUTTON_PRESS_TIMEOUT  (10000)    /* Timeout in milliseconds */
#define WIFI_JOIN_TIMEOUT     (15000)    /* Timeout in milliseconds */

static WiFiEventHandler wifi_connected_handler;
static WiFiEventHandler wifi_disconnected_handler;
static bool connected;

static AsyncWebServer server(80);
static bool button_pressed;
static unsigned long button_pressed_start;


void wifi_connected(const WiFiEventStationModeConnected& event)
{
    Serial.println("Connected to WiFi");
    connected = true;
}

void wifi_disconnected(const WiFiEventStationModeDisconnected& event)
{
    Serial.println("Disonnected from WiFi");
    connected = false;
}

void setup_commissioned()
{
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);

    digitalWrite(LED1_PIN, 1);
    digitalWrite(LED2_PIN, 0);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    pinMode(LEFT_OUTPUT_PIN, OUTPUT);
    digitalWrite(LEFT_OUTPUT_PIN, 0);
    pinMode(RIGHT_OUTPUT_PIN, OUTPUT);
    digitalWrite(RIGHT_OUTPUT_PIN, 0);

    char ssid[64];
    settings_get_ssid(ssid);
    char password[64];
    settings_get_password(password);
    wifi_connected_handler = WiFi.onStationModeConnected(wifi_connected);
    wifi_disconnected_handler = WiFi.onStationModeDisconnected(wifi_disconnected);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wifi: ");
    Serial.println(ssid);

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        connected = true;
    }

    /* Start mDNS server */
    char name[32];
    settings_get_name(name);
    MDNS.begin(name);

    /* Spawn web server */
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send_P(200, "text/html", commissioned_index_html);
        }
    );
    server.begin();
}

void loop_commissioned()
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

    if (connected) {
        /* @todo Send ALIVE message to base station */
    }
}
