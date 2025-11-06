#!/bin/bash
mosquitto_conf='/etc/mosquitto/mosquitto.conf'
sudo apt-get install mosquitto mosquitto-clients
echo "---- Activating mosquitto service"
sudo systemctl enable mosquitto
sleep 2
sudo systemctl status mosquitto
read -p "Lets make a new user for mosquitto (mqtt broker), enter a name: " name
sudo mosquitto_passwd -c /etc/mosquitto/passwd $name
echo "Editing $mosquitto_conf so we listen on port 1883, deny anonymous access and enable password file."
echo "listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd" >> $mosquitto_conf
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
echo "Start your subscription script to upload temperature results to a Google Spreadsheet!"
