#!/bin/sh

if ! [ -x "$(command -v makeself)" ]; then
    echo "Please install makeself"
    exit 1
fi

TMPDIR=$(mktemp -d)

git clone --depth 1 https://github.com/modmypi/PiModules.git "${TMPDIR}/PiModules"
tar czf "${TMPDIR}/PiModules.tar.gz" -C "${TMPDIR}" PiModules
rm -rf "${TMPDIR}/PiModules"

git clone --depth 1 https://github.com/francois-berder/iot-heater.git "${TMPDIR}/heater"
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

[GSM1]
device = /dev/ttyUSB2
incoming = yes
baudrate = 115200
memory_start = 0
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

cp base_station_installer/setup.sh "${TMPDIR}"
chmod 755 "${TMPDIR}/setup.sh"

makeself "${TMPDIR}" base_station_installer.run "Base station installer" ./setup.sh
rm -rf "${TMPDIR}"
