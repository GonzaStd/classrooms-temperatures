#!/bin/bash
mosquitto_conf='/etc/mosquitto/mosquitto.conf'
sudo apt-get install mosquitto mosquitto-clients avahi-daemon
echo "---- Activating mosquitto service"
sudo systemctl enable mosquitto
sleep 2
sudo systemctl status mosquitto
#read -p "Lets make a new user for mosquitto (mqtt broker), enter a name: " name
sudo touch /etc/mosquitto/passwd
sudo mosquitto_passwd -b /etc/mosquitto/passwd raspberrypi Hacemuchocalor
if [ -f $mosquitto_conf]; then
  mv $(mosquitto_conf) "$(mosquitto_conf).bak"
cp ./mosquitto-ct.conf $mosquitto_conf
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
echo "Start your subscription script to upload temperature results to a Google Spreadsheet!";
