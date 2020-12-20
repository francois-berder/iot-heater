# Assembly unit guide for base station

This document is a step by step guide on how to assemble and test base station
device.

## Components required

- Raspberry Pi Zero W
- SIM5320E module
- one micro SD card
- a micro USB to USB-A adapter
- a short micro USB cable
- one 2200uF capacitor (capacitance needs >= 1000uF)
- 2 small wires
- one ABS box

## Assembly

1. Follow the `Raspberry Pi setup` in `base_station/README.md` on how to setup the software on the Raspberry Pi
2. Solder the capacitor on pins 2 and 6 of the Raspberry Pi 40-pin header.
3. Solder one wire from pin 2 of the Raspberry Pi to 5V pin on the SIM5320E
4. Solder one wire from pin 6 of the Raspberry Pi to GND pin on the SIM5320E

The SIM5320E module requires a 4V power supply capable of handling 2A bursts. On the SIM5320E module, a diode lowers the input voltage from 5V to around 4V. Hence, both the Raspberry Pi and the SIM5320E must be powered with a 5V power supply. However, we cannot provide power to the 3G module from the USB connector of the Pi. That is why we need to solder two wires from the Raspberry Pi to the SIM5320E. The large capacitor is there to ensure that the SIM5320E and the Raspberry Pi will not brown when the SIM5320E draws lots of current.

## Power on

1. Connect to the Raspberry Pi via ssh using public/private keys printed while building the installer.
2. Check that SIM5320E is connected by looking at the content of `/dev`. You should see several `/dev/USB<X>` entries.
3. Check that the basestation service is running: `sudo service basestation status`
