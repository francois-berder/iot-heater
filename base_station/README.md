# Base station

## Software

### Build instructions

Type `make` or `BUILDTYPE=debug make` to build `base_station` program.

### Raspberry Pi setup

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

## SMS commands

| SMS                   | Description                                 |
| --------------------- | ------------------------------------------- |
| HELP                  | Reply with basic commands                   |
| PING                  | Reply with `PONG`                           |
| VERSION               | Reply with base station software version    |
| HEATER OFF            | Set heater state to OFF                     |
| HEATER DEFROST        | Set heater state to DEFROST                 |
| HEATER ECO            | Set heater state to ECO                     |
| HEATER COMFORT        | Set heater state to COMFORT                 |
| HEATER ON             | Set heater state to COMFORT                 |
| GET HEATER            | Reply with current heater state             |
| GET IP                | Reply with public IP address                |
| GET HEATER HISTORY    | Reply with previous 16 heater commands      |
| LOCK                  | Only phones whitelisted can send SMS        |
| UNLOCK <pin>          | All phones can send SMS (default PIN: 1234) |
| ADD PHONE <number>    | Add phone number to whitelist               |
| REMOVE PHONE <number> | Remove phone number from whitelist          |

### Phone whitelist

By default, all text messages are parsed by the base station software and commands are executed regardless. This implies that anyone that knows the phone number of your base station can control your heating at home. To counter this threat, specific phones can be whitelisted and any text messages sent from a phone not belonging in the whitelist are discarded.

To use this feature,

1. Add your phone number to the whitelist: `ADD PHONE <your-phone-number>`
2. Send this command to enable this feature: `LOCK`

Any phone can send the `UNLOCK` command (useful if you whitelisted the wrong phone number !).
