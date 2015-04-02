#!/bin/bash

for i in {1..1000000}
do
	echo "[MQTT-0%]"
	mosquitto_pub -h 192.168.1.100 -t "/lights/009FBE4B/dimmer/" -m "O"

	sleep 2
	echo "[MQTT-100%]"
	mosquitto_pub -h 192.168.1.100 -t "/lights/009FBE4B/dimmer/" -m "10O"

	sleep 2
done
