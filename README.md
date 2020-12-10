# Connected heater

**This is an ongoing project, do not expect everything to work !**

The architecture of this solution is as follows: a base station is running and parses incoming SMS. Each heater controller connects to the base station regularly and requests the current heater state.

## Base station

### Hardware

You will need:

- Raspberry Pi Zero W
- SIM5320E with its 3G antenna
- a micro USB to micro USB cable
- some spacers
- microSD card

### Software

Do not plug anything to the Raspberry Pi apart from the microUSB to power the device. Follow these steps:

1. Flash latest Raspbian on a microSD card
2. Create an empty file named `ssh` in `/boot`
3. Add `wpa_supplicant.conf` in /boot with the following content:

```
country=GB
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1

network={
  ssid="NETWORK-NAME"
  psk="NETWORK-PASSWORD"
}
```

4. Disable bluetooth by adding `dtoverlay=disable-bt` in `/boot/config.txt`
5. Build installer by running `base_station_installer/build.sh` and transfer this file to the microSD card in `/home/pi`
6. Boot Pi using micro sdcard
7. Login over SSH
8. Change password by running `passwd`
9. Run installer on Raspberry Pi
10. Shutdown Raspberry pi by running `sudo shutdown now`
11. Power up device

