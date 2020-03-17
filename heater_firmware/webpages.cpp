#include "webpages.h"
#include <Arduino.h>

const char uncommissioned_index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
    <h2>Heater registration</h2>
    <form action="/register" method="post">
        Name:<br>
        <input type="text" name="name" maxlength="31">
        <br>
        Wifi name:<br>
        <input type="text" name="ssid" maxlength="63">
        <br>
        Wifi password:<br>
        <input type="password" name="password" maxlength="63">
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
</head>
<body>
  <h2>Heater</h2>
</body>
</html>
)rawliteral";
