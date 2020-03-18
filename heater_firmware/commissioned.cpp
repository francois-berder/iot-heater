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
#define WIFI_JOIN_TIMEOUT     (15)    /* Timeout in seconds */

static AsyncWebServer server(80);
static bool button_pressed;
static unsigned long button_pressed_start;

void setup_commissioned()
{
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);

    digitalWrite(LED1_PIN, 1);
    digitalWrite(LED2_PIN, 0);

    pinMode(BUTTON_PIN, INPUT);

    pinMode(LEFT_OUTPUT_PIN, OUTPUT);
    digitalWrite(LEFT_OUTPUT_PIN, 0);
    pinMode(RIGHT_OUTPUT_PIN, OUTPUT);
    digitalWrite(RIGHT_OUTPUT_PIN, 0);

    char ssid[64];
    settings_get_ssid(ssid);
    char password[64];
    settings_get_password(password);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wifi: ");
    Serial.println(ssid);
    /* Wait for the WiFi to connect */
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < WIFI_JOIN_TIMEOUT) {
        delay(1000);
        timeout++;
    }

    if (WiFi.status() != WL_CONNECTED && timeout == WIFI_JOIN_TIMEOUT) {
        /* @todo Display error code on LED 2 */
        ESP.restart();
    }

    /* Start mDNS server */
    char name[64];
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
}
