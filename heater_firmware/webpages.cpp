#include "webpages.h"
#include <Arduino.h>

const char uncommissioned_index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Heater registration</title>
</head>
<body>
    <h1>Heater controller</h1>
    <h2>Device information</h2>
    ESP8266 chip ID: %08X
    <br>
    Firmware version: %s
    <br>
    AP MAC address: %02X:%02X:%02X:%02X:%02X:%02X
    <br>
    Uptime: %s
    <h2>Registration</h2>
    <form action="/register" method="post">
        Name:<br>
        <input type="text" name="name" maxlength="31" required="required" pattern="[A-Za-z0-9]+">
        <br>
        WiFi name:<br>
        <input type="text" name="ssid" maxlength="63" required="required">
        <br>
        WiFi password:<br>
        <input type="password" name="password" maxlength="63">
        <br>
        Base station hostname/IP address:<br>
        <input type="text" name="basestation" value="basestation" maxlength="63">
        <br>
        <input type="submit" value="Register">
    </form>
</body>
</html>
)rawliteral";

const char commissioned_index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Heater - %s</title>
</head>
<body>
  <h1>Heater controller - %s</h1>
  <h2>Device information</h2>
  ESP8266 chip ID: %08X
  <br>
  Firmware version: %s
  <br>
  MAC address: %02X:%02X:%02X:%02X:%02X:%02X
  <br>
  Uptime: %s
  <br>
  WiFi signal strength (RSSI): %d dBm
  <br>
  Base station addr: %s
  <br>
  Heater state: %s
  <br>
  Last heater state reply from base station: %u
  <br>
  Error count since boot: %u
  <br>
  <br>
  <form action="/unregister" method="post">
      <button name="unregister" value="unregister">Reset configuration</button>
  </form>
  <h2>Errors</h2>
  %s
</body>
</html>
)rawliteral";
