# Heater firmware

This project aims at controlling an electric heater using an ESP8266 and a custom board. This board connects regularly to a base station to obtain the desired heater state (off, defrost, eco...) and controls the heater via a one-wire connection.

## Requirements

[arduino-cli](https://github.com/arduino/arduino-cli) is used to compile and upload binary to the ESP8266 if you are using the Makefile. If you are using the Arduino IDE, you can skip this step.

Run the following command to install arduino-cli:

```sh
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh
```

Install ESP8266 board in Arduino IDE:
  1. Open File -> Preferences (or Arduino -> Preferences on MacOS)
  2. Enter `http://arduino.esp8266.com/stable/package_esp8266com_index.json` in the Additional Boards Manager URLs field
  3. Go to Tools -> Board -> Boards Manager and install ESP8266 by ESP8266 Community

Arduino library dependencies:

- [ESP8266mDNS](https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266mDNS)
- [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [ESP8266TrueRandom](https://github.com/marvinroger/ESP8266TrueRandom)
- [NTPClient](https://github.com/arduino-libraries/NTPClient)
- [Uptime-Library](https://github.com/YiannisBourkelis/Uptime-Library)

## Build & Flash instructions

You can either use the Arduino IDE to compile and flash the board, or use the Makefile (requires `arduino-cli`) and run these commands:

```sh
make
make upload
```

## Serial port

The firmware opens a serial connection (115200 8N1) over USB which is currently used only for debug. Run this command to compile/upload and get serial output from the board:

```sh
make term
```

## LED

| Status                        | LED                                    |
| ----------------------------- | -------------------------------------- |
| Not commissioned              | LED ON                                 |
| Not commissioned but someone on WiFi | LED flash every 100ms           |
| Not connected to WiFi         | LED flash every 500ms                  |
| Not connected to base station | LED flash every 3 seconds              |
| All OK                        | LED ON only if reset button is pressed |

## Reset button

If the device is in commissioned mode, pressing the reset button for 10 seconds will reset
the device and clear its configuration. The device comes up in uncommissioned mode.
