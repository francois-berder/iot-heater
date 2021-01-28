#!/bin/sh -e

# This script creates an installer that will setup
# the Raspberry Pi. After running the installer
# and rebooting, the Pi acts as a base station.

if ! [ -x "$(command -v makeself)" ]; then
    echo "Please install makeself"
    exit 1
fi

if ! [ -x "$(command -v git)" ]; then
    echo "Please install git"
    exit 1
fi


HEATER_REPO="https://github.com/francois-berder/iot-heater.git"
BRANCH_NAME="master"
while getopts "hr:b:" arg; do
  case $arg in
    h)
      echo "Usage: $0 [-h] [-r git-repository]"
      exit 0
      ;;
    r)
      HEATER_REPO=$OPTARG
      ;;
    b)
      BRANCH_NAME=$OPTARG
      ;;
  esac
done

TMPDIR=$(mktemp -d)

git clone --depth 1 --branch "${BRANCH_NAME}" "${HEATER_REPO}" "${TMPDIR}/heater"
tar czf "${TMPDIR}/heater.tar.gz" -C "${TMPDIR}" heater
rm -rf "${TMPDIR}/heater"

# Generate smstool configuration file
cat > "${TMPDIR}/smsd.conf" <<- EOM
#
# /etc/smsd.conf
#
# Description: Main configuration file for the smsd
#

devices = GSM1
outgoing = /var/spool/sms/outgoing
checked = /var/spool/sms/checked
incoming = /var/spool/sms/incoming
logfile = /var/log/smstools/smsd.log
infofile = /var/run/smstools/smsd.working
pidfile = /var/run/smstools/smsd.pid
outgoing = /var/spool/sms/outgoing
checked = /var/spool/sms/checked
failed = /var/spool/sms/failed
incoming = /var/spool/sms/incoming
sent = /var/spool/sms/sent
stats = /var/log/smstools/smsd_stats
receive_before_send = no
autosplit = 3
whitelist = /etc/smsd.whitelist

[GSM1]
device = /dev/ttyUSB2
incoming = yes
baudrate = 115200
memory_start = 0
EOM

cat > "${TMPDIR}/smsd.whitelist" <<- EOM
33 # Allow any destination from France
44 # Allow any destination from UK
EOM

cat > "${TMPDIR}/basestation.service" <<- EOM
[Unit]
Description=Base station server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/base_station

[Install]
WantedBy=multi-user.target
EOM

# Generate ssh key
ssh-keygen -t rsa -b 2048 -f "${TMPDIR}/base_station_ssh_key" -q -N ""
cat "${TMPDIR}/base_station_ssh_key"
cat "${TMPDIR}/base_station_ssh_key.pub"
rm "${TMPDIR}/base_station_ssh_key"
echo "There are the keys for accessing the base station over SSH. Make sure to save these files."

cat > "${TMPDIR}/setup.sh" <<- 'EOM'
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
apt -y install unattended-upgrades smstools git build-essential i2c-tools python-rpi.gpio python-dev python-serial python-smbus python-jinja2 python-xmltodict python-psutil python-pip dnsutils libmicrohttpd-dev

# Disable HDMI
echo "Disabling HDMI"
sed -i 's/exit 0//' /etc/rc.local
printf "/usr/bin/tvservice -o\nexit 0\n" >> /etc/rc.local

# Configure smstools daemon
echo "Configuring smstools daemon"
install -m 644 smsd.conf /etc/smsd.conf
install -m 644 smsd.whitelist /etc/smsd.whitelist
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
EOM

chmod 755 "${TMPDIR}/setup.sh"

makeself "${TMPDIR}" base_station_installer.run "Base station installer" ./setup.sh
rm -rf "${TMPDIR}"
