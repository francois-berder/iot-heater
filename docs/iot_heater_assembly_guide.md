# Assembly unit guide for IOT-heater

This document is a step by step guide on how to assemble and test a IOT-heater
device.

## Step 1: Soldering components

Solder the following components on the PCB:

- D1 (5mm green LED)
- D3 (1N40005)
- D4 (1N40005)
- SW1 (push button)
- C1 (0.1uF)
- R1 (1k resistor)
- R2 (1k resistor)
- R4 (1k resistor)
- R7 (1k resistor)
- U2 (CPC1540)
- U3 (CPC1540)
- U1 (2x 15pin header)
- 220VAC to 5VDC converter

Pull-down/Pull-up resistors truth table:

|         |  R5  |  R6  |  R8  |  R9  |
| ------- | ---- | ---- | ---- | ---- |
| OFF     |  fit |      |  fit |      |
| CONFORT |      |  fit |      |  fit |
| ECO     |  fit |      |  fit |      |
| DEFROST |      |  fit |      |  fit |

## Step 2: Power converter test

Do **not** plug a ESP8266

1. Power unit from 220VAC
2. Check that LED from power converter is ON
3. With a multimeter, check power converter output is in range 4.8-5.2V

If successful, solder 5V & GND wires (red wire and black wire)

## Step 3: ESP8266 power test

Plug a ESP8266 device in the unit.

1. Plug a ESP8266
2. Power unit from 220VAC
3. Check LED on ESP8266 is solid ON

## Step 4: ESP8266 programming

1. Connect USB cable from PC to ESP8266
2. Download latest release on https://github.com/francois-berder/iot-heater
3. Run `arduino-cli upload -b esp8266:esp8266:nodemcuv2 -p <serial-port> -i <firmware-file>`

## Step 5: Uncommissioned mode test

1. Power unit from 220VAC
2. Check green LED is solid ON
3. Check WiFi network is present
4. Connect to WiFi network
5. Go to http://192.168.0.1/ and check if a webpage can be seen

## Step 6: commissioned mode test

1. Power unit from 220VAC
2. Go to http://192.168.0.1/ and enter WiFi credentials to commission device
3. Check green LED is off
4. Press for once second the reset button and check that green LED is on
5. Power down base station and check green LED flash every 3 seconds
6. Power down WiFi and check green LED flash every 500ms
