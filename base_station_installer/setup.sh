#!/bin/sh -e

# This script assumes that it is running on a Raspberry Pi.
# It should be part of an installer built using
# base_station_installer/build.sh
#
# It setups the Pi as a base station.

if [ "$(id -u)" -ne 0 ]; then
    echo "Please run as root"
    exit 1
fi

TMPDIR=$(mktemp -d)

# Install pubkey
echo "Installing SSH public key"
install -D --mode 644 --owner pi --group pi base_station_ssh_key.pub /home/pi/.ssh/authorized_keys
chown pi:pi /home/pi/.ssh
chmod 700 /home/pi/.ssh

# Disable password authentication
echo "Disabling password authentication over SSH"
sed -i 's/#PasswordAuthentication yes/PasswordAuthentication no/g' /etc/ssh/sshd_config

# Upgrade packages and install dependencies
apt update
apt -y upgrade
apt -y install unattended-upgrades smstools git build-essential i2c-tools python-rpi.gpio python-dev python-serial python-smbus python-jinja2 python-xmltodict python-psutil python-pip dnsutils

# Disable HDMI
echo "Disabling HDMI"
sed -i 's/exit 0//' /etc/rc.local
printf "/usr/bin/tvservice -o\nexit 0\n" >> /etc/rc.local

# Configure smstools daemon
echo "Configuring smstools daemon"
install -m 644 smsd.conf /etc/smsd.conf
service smstools restart

# Install base station server
echo "Installing base station server"
tar xf heater.tar.gz -C "${TMPDIR}"
make -C "${TMPDIR}/heater/base_station"
install -m 755 "${TMPDIR}/heater/base_station/build/release/bin/base_station-release" /usr/local/bin/base_station
install -m 644 basestation.service /lib/systemd/system/basestation.service
systemctl enable basestation
systemctl start basestation

# Change hostname
echo "Setting hostname"
echo "127.0.0.1       basestation" >> /etc/hosts
echo basestation > /etc/hostname

echo "Setup done. Reboot device."
