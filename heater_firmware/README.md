# Heater firmware

This project aims at controlling an electric heater using an ESP8266 and a custom board. This board connects regularly to a base station to obtain the desired heater state (off, defrost, eco...) and controls the heater via a one-wire connection.

## Requirements

[arduino-cli](https://github.com/arduino/arduino-cli) is used to compile and upload binary to the ESP8266.

Run the following command to install arduino-cli:

```sh
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh
```

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

## LEDs

| Status                        | LED                                             |
| ----------------------------- | ----------------------------------------------- |
| Not commissioned              | Alternating LED1 and LED2 flashing every second |
| Commissioning                 | Alternating LED1 and LED2 flashing every 100ms  |
| Not connected to WiFi         | LED1 flashing every 500ms and LED 2 off         |
| Not connected to base station | LED1 off and LED2 flashing every 500ms          |
| All OK                        | LED1 and LED2 flashing every 10 seconds         |
