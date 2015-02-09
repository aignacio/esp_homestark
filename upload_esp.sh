#!/bin/bash

ESPTOOL=/home/aignacio/esptool/esptool.py
ESPPORT=/dev/ttyUSB0
#ESP_FOLDER=esp_homestark
ESP_FOLDER=esphttpd
$ESPTOOL --port $ESPPORT write_flash 0x00000 $ESP_FOLDER/firmware/0x00000.bin
sleep 3
$ESPTOOL --port $ESPPORT write_flash 0x40000 $ESP_FOLDER/firmware/0x40000.bin
sleep 3
$ESPTOOL --port $ESPPORT write_flash 0x12000 $ESP_FOLDER/webpages.espfs
