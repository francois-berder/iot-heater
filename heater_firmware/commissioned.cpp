#include "board.h"
#include "commissioned.h"
#include "heater.h"
#include "settings.h"
#include "webpages.h"
#include "Arduino.h"
#include "Ticker.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define BUTTON_PRESS_TIMEOUT  (10000)    /* Timeout in milliseconds */
#define WIFI_JOIN_TIMEOUT     (15000)    /* Timeout in milliseconds */
#define HEATER_STATE_TIMEOUT  (1000)

static WiFiEventHandler wifi_connected_handler;
static WiFiEventHandler wifi_disconnected_handler;
static WiFiEventHandler wifi_got_ip_handler;
static bool connected;

static AsyncWebServer server(80);
static char webpage_buffer[512];
static bool button_pressed;
static unsigned long button_pressed_start;

#define SEND_ALIVE_PERIOD           (30000)     /* in milliseconds */

#define SEND_ALIVE_EV               (1)

static Ticker send_alive_ticker;
static uint32_t events;

#define BLINK_PERIOD           (500)    /* in milliseconds */
static Ticker leds_ticker;

enum led_state_t {
    DISCONNECTED_FROM_WIFI,
    DISCONNECTED_FROM_BASE_STATION,
    CONNECTED_TO_BASE_STATION,
};
enum led_state_t led_state;

#define base_station_hostname       "basestation.lan"
#define BASE_STATION_PORT           (32322)
static unsigned int base_station_failure;
#define MAX_BASE_STATION_FAILURE    (15)

static uint8_t heater_state;

enum message_type_t {
    MESSAGE_ALIVE,
    MESSAGE_HEATER_STATE,
};

static void update_leds()
{
    static int counter = 0;
    switch (led_state) {
    case DISCONNECTED_FROM_WIFI:
        digitalWrite(LED1_PIN, !digitalRead(LED1_PIN));
        counter = 0;
        break;
    case DISCONNECTED_FROM_BASE_STATION:
        if (counter >= 6) {
            digitalWrite(LED1_PIN, 1);
            counter = 0;
        } else {
            digitalWrite(LED1_PIN, 0);
        }
        break;
    case CONNECTED_TO_BASE_STATION:
        digitalWrite(LED1_PIN, digitalRead(BUTTON_PIN));
        break;
    };

    ++counter;
}

static void wifi_connected(const WiFiEventStationModeConnected& event)
{
    Serial.println("Connected to WiFi");
    connected = true;
    events |= SEND_ALIVE_EV;
    led_state = DISCONNECTED_FROM_BASE_STATION;
}

static void wifi_disconnected(const WiFiEventStationModeDisconnected& event)
{
    Serial.println("Disonnected from WiFi");
    connected = false;
    led_state = DISCONNECTED_FROM_WIFI;
}

static void wifi_got_ip(const WiFiEventStationModeGotIP& event)
{
    char name[32];
    settings_get_name(name);

    if (!MDNS.begin(name))
        Serial.println("Cannot start MDNS server");
}

static void send_alive_callback(void)
{
    events |= SEND_ALIVE_EV;
}

static void apply_heater_state(void)
{
    switch (heater_state) {
    case HEATER_DEFROST:
        Serial.println("Heater in defrost mode");
        digitalWrite(POSITIVE_OUTPUT_PIN, 0);
        digitalWrite(NEGATIVE_OUTPUT_PIN, 1);
        break;
    case HEATER_ECO:
        Serial.println("Heater in eco mode");
        digitalWrite(POSITIVE_OUTPUT_PIN, 1);
        digitalWrite(NEGATIVE_OUTPUT_PIN, 1);
        break;
    case HEATER_COMFORT:
        Serial.println("Heater in comfort mode");
        digitalWrite(POSITIVE_OUTPUT_PIN, 0);
        digitalWrite(NEGATIVE_OUTPUT_PIN, 0);
        break;
    default:
        Serial.println("Turning heater off");
        digitalWrite(POSITIVE_OUTPUT_PIN, 1);
        digitalWrite(NEGATIVE_OUTPUT_PIN, 0);
        break;
    }
}

void setup_commissioned()
{
    pinMode(LED1_PIN, OUTPUT);
    digitalWrite(LED1_PIN, 1);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    pinMode(POSITIVE_OUTPUT_PIN, OUTPUT);
    pinMode(NEGATIVE_OUTPUT_PIN, OUTPUT);

    led_state = DISCONNECTED_FROM_WIFI;
    leds_ticker.attach_ms(BLINK_PERIOD, update_leds);

    /*
     * Turn off heater. This is the safest option as
     * we do not know how long we stayed off
     * and whether the base station is up and running. If it is,
     * we will soon get the heater state.
     */
    heater_state = HEATER_OFF;
    apply_heater_state();

    char ssid[64];
    settings_get_ssid(ssid);
    char password[64];
    settings_get_password(password);
    wifi_connected_handler = WiFi.onStationModeConnected(wifi_connected);
    wifi_disconnected_handler = WiFi.onStationModeDisconnected(wifi_disconnected);
    wifi_got_ip_handler = WiFi.onStationModeGotIP(wifi_got_ip);
    WiFi.mode(WIFI_STA);
    char name[32];
    settings_get_name(name);
    WiFi.hostname(name);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wifi: ");
    Serial.println(ssid);

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        connected = true;
    }

    /* Spawn web server */
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        switch (heater_state) {
        case HEATER_DEFROST:
            sprintf(webpage_buffer, commissioned_index_html, "DEFROST");
            break;
        case HEATER_ECO:
            sprintf(webpage_buffer, commissioned_index_html, "ECO");
            break;
        case HEATER_COMFORT:
            sprintf(webpage_buffer, commissioned_index_html, "COMFORT");
            break;
        default:
            sprintf(webpage_buffer, commissioned_index_html, "OFF");
            break;
        }
            request->send_P(200, "text/html", webpage_buffer);
        }
    );
    server.begin();

    send_alive_ticker.attach_ms(SEND_ALIVE_PERIOD, send_alive_callback);
}

void loop_commissioned()
{
    MDNS.update();

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

    if (connected && (events & SEND_ALIVE_EV)) {
        events &= ~SEND_ALIVE_EV;

        WiFiClient client;
        if (client.connect(base_station_hostname, BASE_STATION_PORT)) {
            uint8_t message[64];

            Serial.println("Send alive message");

            /* Send alive */
            message[0] = MESSAGE_ALIVE;
            message[1] = MESSAGE_ALIVE >> 8;
            memset(&message[2], 0xFF, sizeof(message) - 2);
            client.write(message, sizeof(message));
            base_station_failure = 0;

            unsigned long start = millis();
            while (millis() - start < HEATER_STATE_TIMEOUT) {
                if (client.available() >= 64)
                    break;
            }
            if (millis() - start > HEATER_STATE_TIMEOUT) {
                Serial.println("Timeout receiving heater state");
            } else {
                client.read(message, sizeof(message));
                uint16_t type;
                type = (message[1] << 8) | message[0];

                if (type == MESSAGE_HEATER_STATE) {
                    heater_state = message[2];
                    apply_heater_state();
                } else {
                    Serial.print("Received invalid message type ");
                    Serial.print(type);
                    Serial.println(" from base station");
                }

                led_state = CONNECTED_TO_BASE_STATION;
            }

        } else {
            Serial.println("Failed to connect to base station");
            if (base_station_failure < MAX_BASE_STATION_FAILURE)
                base_station_failure++;

            if (base_station_failure == MAX_BASE_STATION_FAILURE) {
                Serial.println("Too many failures while sending ALIVE to base station");

                /*
                 * It seems that the base station is down. Turn off
                 * the heater.
                 */
                heater_state = HEATER_OFF;
                apply_heater_state();

                led_state = DISCONNECTED_FROM_BASE_STATION;
            }
        }

        client.stop();
    }

    wifi_set_sleep_type(LIGHT_SLEEP_T);
    delay(200);
}
