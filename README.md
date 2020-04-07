# Connected heater

**This is an ongoing project, do not expect everything to work !**

The architecture of this solution is as follows: a base station is running and parses incoming SMS. Each heater controller connects to the base station regularly and requests the current heater state.

## Base station

### Hardware

You will need:

- Raspberry Pi Zero W
- SIM5320E with its 3G antenna
- UPS PICO hat
- a micro USB to micro USB
- some spacers
- microSD card
- 40-pin header to connect UPS pico hat to Raspberry Pi

### Software

1. Flash latest Raspbian on a microSD card
2. Create an empty file named ssh in /boot
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

4. Boot Pi using micro sdcard
5. Login over SSH
6. Create `.ssh` directory, and create file `authorized_keys` in this directory with the pubkey inside this file
7. Disable password authentication by `/etc/ssh/sshd_config` and uncomment `PasswordAuthentication no`
8. Restart sshd service `sudo service sshd restart`
9. Change password
10. Disable bluetooth. Add `dtoverlay=disable-bt` in `/boot/config.txt` and disable service: `sudo systemctl disable hciuart`.
11. Upgrade packages: `sudo apt update`. `sudo apt upgrade`
12. Install packages: `sudo apt install -y unattended-upgrades smstools git build-essential i2c-tools python-rpi.gpio python-dev python-serial python-smbus python-jinja2 python-xmltodict python-psutil python-pip`
13. Configure smsd. Open `/etc/smsd.conf` and change settings for GSM1 to:

```
[GSM1]
#init =
device = /dev/ttyUSB2
incoming = yes
#pin =
baudrate = 115200
```

14. Restart smsd: `sudo service smstools restart`
15. Disable HDMI by adding `/usr/bin/tvservice -o` to `/etc/rc.local`
16. Change hostname. Open `/etc/hosts` and replace last line with: `127.0.0.1	homegateway`. Open `/etc/hostname` and replace content with `homegateway`
17. Enable I2C by running `sudo raspi-config`
18. Install pico fss daemon:

```sh
git clone https://github.com/modmypi/PiModules.git ~/PiModules
cd ~/PiModules
sudo python setup.py install
sudo update-rc.d picofssd defaults
sudo update-rc.d picofssd enable
```

19. Add `i2c-dev` and `rtc-ds1307` in `/etc/modules`.
20. Add the following lines in `/etc/rc.local`:

```sh
echo ds1307 0x68 > /sys/class/i2c-adapter/i2c-1/new_device
( sleep 4; hwclock -s ) &
```

21. Reboot
