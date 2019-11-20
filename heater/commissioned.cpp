#include "board.h"
#include "commissioned.h"
#include "settings.h"
#include "Arduino.h"
#include <ESP8266WiFi.h>

#define WIFI_JOIN_TIMEOUT     (15)    /* Timeout in seconds */

static void button_isr()
{
    /* @todo Not yet implemented */
}

void setup_commissioned()
{
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);

    digitalWrite(LED1_PIN, 1);
    digitalWrite(LED2_PIN, 0);

    pinMode(BUTTON_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_isr, CHANGE);

    pinMode(LEFT_OUTPUT_PIN, OUTPUT);
    digitalWrite(LEFT_OUTPUT_PIN, 0);
    pinMode(RIGHT_OUTPUT_PIN, OUTPUT);
    digitalWrite(RIGHT_OUTPUT_PIN, 0);

    char ssid[64];
    settings_get_ssid(ssid);
    char password[64];
    settings_get_password(password);
    WiFi.begin(ssid, password);
    /* Wait for the WiFi to connect */
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < WIFI_JOIN_TIMEOUT) {
        delay(1000);
        timeout++;
    }

    if (WiFi.status() != WL_CONNECTED && timeout == WIFI_JOIN_TIMEOUT) {
        /* @todo Display error code on LED 2 */
        while (1);
    }
}

void loop_commissioned()
{
    /* @todo Not yet implemented */
}
