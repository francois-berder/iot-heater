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
#include <ESP8266TrueRandom.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define BUTTON_PRESS_TIMEOUT  (10000)    /* Timeout in milliseconds */
#define WIFI_JOIN_TIMEOUT     (15000)    /* Timeout in milliseconds */
#define HEATER_STATE_TIMEOUT  (1000)

#define NTP_ADDRESS  "europe.pool.ntp.org"

static WiFiEventHandler wifi_connected_handler;
static WiFiEventHandler wifi_disconnected_handler;
static WiFiEventHandler wifi_got_ip_handler;
static bool connected;

static AsyncWebServer server(80);
static char webpage_buffer[512];
static bool button_pressed;
static unsigned long button_pressed_start;

static WiFiUDP ntpUDP;
static NTPClient ntpClient(ntpUDP, NTP_ADDRESS);

#define SEND_HEATER_STATE_REQ_PERIOD    (30000)     /* in milliseconds */

#define SEND_HEATER_STATE_REQ_EV        (1)

static Ticker send_heater_state_req_ticker;
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

static uint64_t msg_counter;
enum message_type_t {
    REQ_HEATER_STATE    = 1,
    HEATER_STATE_REPLY  = 2,
};

struct message_t {
    struct __attribute__((packed)) message_header_t {
        uint8_t protocol_version;
        uint8_t msg_type;
        uint8_t mac[6];
        uint64_t counter;
    } header;
    uint8_t data[48];
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

static void log_to_serial(char *str)
{
    char buffer[256];
    unsigned long timestamp;

    timestamp = ntpClient.getEpochTime();
    sprintf(buffer, "[%u] %s", timestamp, str);
    Serial.println(buffer);
}

static void wifi_connected(const WiFiEventStationModeConnected& event)
{
    log_to_serial("Connected to WiFi");
    connected = true;
    events |= SEND_HEATER_STATE_REQ_EV;
    led_state = DISCONNECTED_FROM_BASE_STATION;
}

static void wifi_disconnected(const WiFiEventStationModeDisconnected& event)
{
    log_to_serial("Disonnected from WiFi");
    connected = false;
    led_state = DISCONNECTED_FROM_WIFI;
}

static void wifi_got_ip(const WiFiEventStationModeGotIP& event)
{
    char name[32];
    settings_get_name(name);

    if (!MDNS.begin(name))
        log_to_serial("Cannot start MDNS server");
}

static void send_heater_state_req_callback(void)
{
    events |= SEND_HEATER_STATE_REQ_EV;
}

static void apply_heater_state(void)
{
    switch (heater_state) {
    case HEATER_DEFROST:
        log_to_serial("Heater in defrost mode");
        digitalWrite(POSITIVE_OUTPUT_PIN, 0);
        digitalWrite(NEGATIVE_OUTPUT_PIN, 1);
        break;
    case HEATER_ECO:
        log_to_serial("Heater in eco mode");
        digitalWrite(POSITIVE_OUTPUT_PIN, 1);
        digitalWrite(NEGATIVE_OUTPUT_PIN, 1);
        break;
    case HEATER_COMFORT:
        log_to_serial("Heater in comfort/on mode");
        digitalWrite(POSITIVE_OUTPUT_PIN, 0);
        digitalWrite(NEGATIVE_OUTPUT_PIN, 0);
        break;
    default:
        log_to_serial("Turning heater off");
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

    if (settings_check()) {
        log_to_serial("Settings check fail. Restarting in uncommissioned mode.");
        settings_erase();
        ESP.restart();
    }

    msg_counter = ESP8266TrueRandom.random();
    msg_counter <<= 32;

    led_state = DISCONNECTED_FROM_WIFI;
    leds_ticker.attach_ms(BLINK_PERIOD, update_leds);

    /*
     * Put heater in defrost mode. This is the safest option as
     * we do not know how long we stayed off
     * and whether the base station is up and running. If it is,
     * we will soon get the heater state.
     */
    heater_state = HEATER_DEFROST;
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
    {
        char buffer[128];
        sprintf(buffer, "Connecting to Wifi: %s", ssid);
        Serial.println(buffer);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        connected = true;
    }

    ntpClient.begin();

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
            sprintf(webpage_buffer, commissioned_index_html, "COMFORT/ON");
            break;
        default:
            sprintf(webpage_buffer, commissioned_index_html, "OFF");
            break;
        }
            request->send_P(200, "text/html", webpage_buffer);
        }
    );
    server.begin();

    send_heater_state_req_ticker.attach_ms(SEND_HEATER_STATE_REQ_PERIOD, send_heater_state_req_callback);
}

void loop_commissioned()
{
    ntpClient.update();
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

    if (connected && (events & SEND_HEATER_STATE_REQ_EV)) {
        events &= ~SEND_HEATER_STATE_REQ_EV;

        WiFiClient client;
        if (client.connect(base_station_hostname, BASE_STATION_PORT)) {
            log_to_serial("Sending heater state request");

            struct message_t heater_state_req_msg;
            memset(&heater_state_req_msg, 0xFF, sizeof(heater_state_req_msg));
            heater_state_req_msg.header.protocol_version = 1;
            heater_state_req_msg.header.msg_type = REQ_HEATER_STATE;
            WiFi.macAddress(heater_state_req_msg.header.mac);
            heater_state_req_msg.header.counter = msg_counter++;
            settings_get_name((char *)heater_state_req_msg.data);

            client.write((uint8_t *)&heater_state_req_msg, sizeof(heater_state_req_msg));
            base_station_failure = 0;

            unsigned long start = millis();
            while (millis() - start < HEATER_STATE_TIMEOUT) {
                if (client.available() >= 64)
                    break;
            }
            if (millis() - start > HEATER_STATE_TIMEOUT) {
                log_to_serial("Timeout while waiting for heater state reply from base station");
            } else {
                struct message_t heater_state_reply_msg;
                client.read((uint8_t *)&heater_state_reply_msg, sizeof(heater_state_reply_msg));

                if (heater_state_reply_msg.header.protocol_version != 1) {
                    log_to_serial("Discarding message: protocol version not supported");
                } else if (heater_state_reply_msg.header.msg_type == REQ_HEATER_STATE) {
                    log_to_serial("Discarding message: not expecting REQ_HEATER_STATE from base station");
                } else if (heater_state_reply_msg.header.msg_type == HEATER_STATE_REPLY) {
                    uint8_t new_heater_state = heater_state_reply_msg.data[0];
                    if (heater_state != new_heater_state) {
                        switch (new_heater_state) {
                        case HEATER_OFF:
                        case HEATER_DEFROST:
                        case HEATER_ECO:
                        case HEATER_COMFORT:
                            heater_state = new_heater_state;
                            apply_heater_state();
                            break;
                        default:
                            log_to_serial("Received invalid heater state from base station");
                            break;
                        }
                    }
                } else {
                    led_state = CONNECTED_TO_BASE_STATION;
                }
            }
        } else {
            log_to_serial("Failed to connect to base station");
            if (base_station_failure < MAX_BASE_STATION_FAILURE)
                base_station_failure++;

            if (base_station_failure == MAX_BASE_STATION_FAILURE) {
                log_to_serial("Too many failures while requesting heater state from base station");

                /*
                 * It seems that the base station is down.
                 * Let's put the heater in defrost mode.
                 */
                heater_state = HEATER_DEFROST;
                apply_heater_state();

                led_state = DISCONNECTED_FROM_BASE_STATION;
            }
        }

        client.stop();
    }

    wifi_set_sleep_type(LIGHT_SLEEP_T);
    delay(200);
}
