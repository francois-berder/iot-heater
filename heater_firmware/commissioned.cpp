#include "board.h"
#include "commissioned.h"
#include "heater.h"
#include "settings.h"
#include "version.h"
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

#define BUTTON_PRESS_TIMEOUT  (10000)    /* in milliseconds */
#define WIFI_JOIN_TIMEOUT     (15000)    /* in milliseconds */
#define HEATER_STATE_TIMEOUT  (1000)     /* in milliseconds */

static WiFiEventHandler wifi_connected_handler;
static WiFiEventHandler wifi_disconnected_handler;
static WiFiEventHandler wifi_got_ip_handler;

#define WEB_SERVER_PORT       (80)
static AsyncWebServer server(WEB_SERVER_PORT);
static char webpage_buffer[1024];
static bool button_pressed;
static unsigned long button_pressed_start;

#define NTP_UPDATE_INTERVAL             (15 * 60 * 1000)  /* in milliseconds */
static WiFiUDP ntpUDP;
static NTPClient ntpClient(ntpUDP);
static unsigned long last_heater_state_timestamp;

#define SEND_HEATER_STATE_REQ_PERIOD    (60 * 1000)     /* in milliseconds */

#define SEND_HEATER_STATE_REQ_EV        (1U << 0)

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

#define BASE_STATION_HOSTNAME       "basestation"
#define BASE_STATION_PORT           (32322)
static unsigned int request_state_failure_count;
#define REQUEST_STATE_FAILURE_THRESHOLD    (15)
static unsigned int request_state_failure_since_boot_counter;

static uint8_t heater_state;
#define DEFAULT_HEATER_STATE        (HEATER_DEFROST)

static uint64_t msg_counter;
enum message_type_t {
    REQ_HEATER_STATE    = 1,
    HEATER_STATE_REPLY  = 2,
};

/* 64-byte message */
struct __attribute__((packed)) message_t {
    struct __attribute__((packed)) message_header_t {
        uint8_t protocol_version;
        uint8_t msg_type;
        byte mac[6];
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
        digitalWrite(LED1_PIN, !digitalRead(BUTTON_PIN));
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
    events |= SEND_HEATER_STATE_REQ_EV;
    led_state = DISCONNECTED_FROM_BASE_STATION;
}

static void wifi_disconnected(const WiFiEventStationModeDisconnected& event)
{
    log_to_serial("Disonnected from WiFi");
    led_state = DISCONNECTED_FROM_WIFI;
}

static void wifi_got_ip(const WiFiEventStationModeGotIP& event)
{
    char buf[40];
    char buf2[32];
    WiFi.localIP().toString().toCharArray(buf2, sizeof(buf2) - 1);
    strcpy(buf, "IP: ");
    strcat(buf, buf2);
    log_to_serial(buf);
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
        log_to_serial("Heater set in defrost mode");
        digitalWrite(POSITIVE_OUTPUT_PIN, 0);
        digitalWrite(NEGATIVE_OUTPUT_PIN, 1);
        break;
    case HEATER_ECO:
        log_to_serial("Heater set in eco mode");
        digitalWrite(POSITIVE_OUTPUT_PIN, 1);
        digitalWrite(NEGATIVE_OUTPUT_PIN, 1);
        break;
    case HEATER_COMFORT:
        log_to_serial("Heater set in comfort/on mode");
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
    /* Init pins */
    pinMode(LED1_PIN, OUTPUT);
    digitalWrite(LED1_PIN, 1);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    pinMode(POSITIVE_OUTPUT_PIN, OUTPUT);
    pinMode(NEGATIVE_OUTPUT_PIN, OUTPUT);
    /*
     * Put heater in defrost mode. This is the safest option as
     * we do not know how long we stayed off
     * and whether the base station is up and running. If it is,
     * we will soon get the heater state.
     */
    heater_state = DEFAULT_HEATER_STATE;
    apply_heater_state();

    /* Load settings */
    if (!settings_check()) {
        log_to_serial("Settings check fail. Restarting in uncommissioned mode.");
        settings_erase();
        ESP.restart();
    } else {
        log_to_serial("Settings check OK.");
    }

    char name[32];
    char ssid[64];
    char password[64];
    settings_get_name(name);
    settings_get_ssid(ssid);
    settings_get_password(password);

    {
      char buffer[64];
      sprintf(buffer, "Heater controller name: \"%s\"", name);
      log_to_serial(buffer);
    }

    /* Init message counter */
    {
      msg_counter = ESP8266TrueRandom.random();
      msg_counter &= 0x0FFFFFFF;  /* Clear highest 4 bits, to ensure that msg_counter will not overflow */
      msg_counter <<= 32;
      char buffer[64];
      sprintf(buffer, "Message counter set to %llu", msg_counter);
      log_to_serial(buffer);
    }
    request_state_failure_since_boot_counter = 0;

    led_state = DISCONNECTED_FROM_WIFI;
    leds_ticker.attach_ms(BLINK_PERIOD, update_leds);

    /* Init WiFi */
    Serial.println(String("MAC address: ") + String(WiFi.macAddress()));
    wifi_connected_handler = WiFi.onStationModeConnected(wifi_connected);
    wifi_disconnected_handler = WiFi.onStationModeDisconnected(wifi_disconnected);
    wifi_got_ip_handler = WiFi.onStationModeGotIP(wifi_got_ip);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(String("heater-") + name);
    WiFi.begin(ssid, password);
    {
      char buffer[64];
      sprintf(buffer, "Connecting to Wifi network: \"%s\"", ssid);
      log_to_serial(buffer);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }

    ntpClient.setUpdateInterval(NTP_UPDATE_INTERVAL);
    ntpClient.begin();

    /* Spawn web server */
    server.on("/", HTTP_GET, [name](AsyncWebServerRequest *request){
        char heater_state_str[16];
        switch (heater_state) {
        case HEATER_DEFROST: strcpy(heater_state_str, "DEFROST"); break;
        case HEATER_ECO: strcpy(heater_state_str, "ECO"); break;
        case HEATER_COMFORT: strcpy(heater_state_str, "COMFORT/ON"); break;
        case HEATER_OFF: strcpy(heater_state_str, "OFF"); break;
        default: strcpy(heater_state_str, "UNKNOWN"); break;
        }
        byte mac[6];
        WiFi.macAddress(mac);
        sprintf(webpage_buffer, commissioned_index_html,
                    name,
                    ESP.getChipId(),
                    FW_VERSION,
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                    heater_state_str,
                    last_heater_state_timestamp,
                    request_state_failure_since_boot_counter);
        request->send_P(200, "text/html", webpage_buffer);
        }
    );
    server.begin();

    /* Add a little bit of jitter so that not all heater controllers
     *  are contacting the base station at the same time. This scenario
     *  is likely to happen when you get a power cut and it comes back
     *  on.
     */
    send_heater_state_req_ticker.attach_ms(
      SEND_HEATER_STATE_REQ_PERIOD + 50 * ((msg_counter >> 32) & 0xF),
      send_heater_state_req_callback);
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
            int i;

            log_to_serial("Factory reset");
            settings_erase();

            /*
             * Flash a bit the LEDs a few times to provide some feedback
             * to the user that the device is restarting.
             */
            digitalWrite(LED1_PIN, 0);
            for (i = 0; i < 3; ++i) {
              delay(100);
              digitalWrite(LED1_PIN, !digitalRead(LED1_PIN));
            }

            ESP.restart();
        }
    } else {
        button_pressed = false;
    }

    if (WiFi.status() == WL_CONNECTED && (events & SEND_HEATER_STATE_REQ_EV)) {
        events &= ~SEND_HEATER_STATE_REQ_EV;

        WiFiClient client;
        if (client.connect(BASE_STATION_HOSTNAME, BASE_STATION_PORT)) {
            log_to_serial("Sending heater state request to base station");

            struct message_t heater_state_req_msg;
            memset(&heater_state_req_msg, 0xFF, sizeof(heater_state_req_msg));
            heater_state_req_msg.header.protocol_version = 1;
            heater_state_req_msg.header.msg_type = REQ_HEATER_STATE;
            WiFi.macAddress(heater_state_req_msg.header.mac);
            heater_state_req_msg.header.counter = msg_counter++;
            settings_get_name((char *)heater_state_req_msg.data);

            client.write((uint8_t *)&heater_state_req_msg, sizeof(heater_state_req_msg));

            unsigned long start = millis();
            while (millis() - start < HEATER_STATE_TIMEOUT) {
                if (client.available() >= 64)
                    break;
            }
            if (millis() - start > HEATER_STATE_TIMEOUT) {
                log_to_serial("Timeout while waiting for heater state reply from base station");
                request_state_failure_count++;
                request_state_failure_since_boot_counter++;
            } else {
                struct message_t heater_state_reply_msg;
                client.read((uint8_t *)&heater_state_reply_msg, sizeof(heater_state_reply_msg));

                if (heater_state_reply_msg.header.protocol_version != 1) {
                    char buffer[128];
                    sprintf(buffer, "Discarding message: protocol version %u not supported", heater_state_reply_msg.header.protocol_version);
                    log_to_serial(buffer);
                    request_state_failure_count++;
                    request_state_failure_since_boot_counter++;
                } else if (heater_state_reply_msg.header.msg_type == REQ_HEATER_STATE) {
                    log_to_serial("Discarding message: not expecting REQ_HEATER_STATE from base station");
                    request_state_failure_count++;
                    request_state_failure_since_boot_counter++;
                } else if (heater_state_reply_msg.header.msg_type != HEATER_STATE_REPLY) {
                    char buffer[128];
                    sprintf(buffer, "Invalid message type %u", heater_state_reply_msg.header.msg_type);
                    log_to_serial(buffer);
                    request_state_failure_count++;
                    request_state_failure_since_boot_counter++;
                } else {
                    uint8_t new_heater_state = heater_state_reply_msg.data[0];
                    switch (new_heater_state) {
                    case HEATER_OFF:
                    case HEATER_DEFROST:
                    case HEATER_ECO:
                    case HEATER_COMFORT:
                        led_state = CONNECTED_TO_BASE_STATION;
                        last_heater_state_timestamp = ntpClient.getEpochTime();
                        request_state_failure_count = 0;
                        if (heater_state != new_heater_state) {
                            heater_state = new_heater_state;
                            apply_heater_state();
                        }
                        break;
                    default:
                        {
                          char buffer[64];
                          sprintf(buffer, "Received invalid heater state %d from base station", new_heater_state);
                          log_to_serial(buffer);
                          request_state_failure_count++;
                          request_state_failure_since_boot_counter++;
                        }
                        break;
                    }
                }
            }
        } else {
            log_to_serial("Failed to connect to base station");
            request_state_failure_count++;
            request_state_failure_since_boot_counter++;
        }

        client.stop();
    }

    if (request_state_failure_count == REQUEST_STATE_FAILURE_THRESHOLD) {
        char buffer[128];
        sprintf(buffer, "Too many failures (count: %d) while requesting heater state from base station", request_state_failure_count);
        log_to_serial(buffer);

        /*
         * It seems that the base station is down or not running properly.
         * Let's put the heater in defrost mode.
         */
        heater_state = DEFAULT_HEATER_STATE;
        apply_heater_state();

        led_state = DISCONNECTED_FROM_BASE_STATION;
    }

    wifi_set_sleep_type(LIGHT_SLEEP_T);
    delay(200);
}
