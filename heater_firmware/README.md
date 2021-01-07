# Heater firmware

This project aims at controlling an electric heater using an ESP8266 and a custom board. This board connects regularly to a base station to obtain the desired heater state (off, defrost, eco...) and controls the heater via a one-wire connection.

## Requirements

[arduino-cli](https://github.com/arduino/arduino-cli) is used to compile and upload binary to the ESP8266.

Run the following command to install arduino-cli:

```sh
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh
```

Arduino library dependencies:

- [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [ESP8266TrueRandom](https://github.com/marvinroger/ESP8266TrueRandom)
- [NTPClient](https://github.com/arduino-libraries/NTPClient)

## Build & Flash instructions

To compile and flash the board, use these commands:

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
| Not connected to WiFi         | LED flash every 500ms                  |
| Not connected to base station | LED flash every 3 seconds              |
| All OK                        | LED ON only if reset button is pressed |

## Reset button

If the device is in commissioned mode, pressing the reset button for 10 seconds will reset
the device and clear its configuration. The device comes up in uncommissioned mode.
