#!/usr/bin/env bash

#sudo apt-get update
sudo apt-get install -y git autoconf build-essential gperf bison flex texinfo libtool libncurses5-dev wget gawk libc6-dev python-serial libexpat-dev unzip

sudo rm -rf /opt/Espressif
sudo mkdir -p /opt/Espressif
sudo chown -R $USER /opt/Espressif

#Install crosstool-NG (toolchain)
cd /opt/Espressif
git clone -b lx106 http://github.com/jcmvbkbc/crosstool-NG.git 
cd crosstool-NG
./bootstrap && ./configure --prefix=`pwd` && make && make install
./ct-ng xtensa-lx106-elf
./ct-ng build

echo "export PATH=$PWD/builds/xtensa-lx106-elf/bin:\$PATH" | sudo tee -a /root/.bashrc
echo "export PATH=$PWD/builds/xtensa-lx106-elf/bin:\$PATH" >> ~/.bashrc

#Setup SDK
cd /opt/Espressif
wget -O esp_esp_iot_sdk_v0.9.5_15_01_23.zip https://github.com/esp8266/esp8266-wiki/raw/master/sdk/esp_iot_sdk_v0.9.5_15_01_23.zip
wget -O esp_iot_sdk_v0.9.5_15_01_23_patch1.zip https://github.com/esp8266/esp8266-wiki/raw/master/sdk/esp_iot_sdk_v0.9.5_15_01_23_patch1.zip
unzip esp_esp_iot_sdk_v0.9.5_15_01_23.zip
unzip -o esp_iot_sdk_v0.9.5_15_01_23_patch1.zip
ln -s esp_iot_sdk_v0.9.5 ESP8266_SDK

#Add missing libs
cd /opt/Espressif/ESP8266_SDK
wget -O lib/libc.a https://github.com/esp8266/esp8266-wiki/raw/master/libs/libc.a
wget -O lib/libhal.a https://github.com/esp8266/esp8266-wiki/raw/master/libs/libhal.a
wget -O include.tgz https://github.com/esp8266/esp8266-wiki/raw/master/include.tgz
tar -xvzf include.tgz

#Install esptool
cd /opt/Espressif
wget -O esptool_0.0.2-1_i386.deb https://github.com/esp8266/esp8266-wiki/raw/master/deb/esptool_0.0.2-1_i386.deb
sudo dpkg -i esptool_0.0.2-1_i386.deb

#Install esptool.py
cd /opt/Espressif
git clone https://github.com/themadinventor/esptool esptool-py
sudo ln -s $PWD/esptool-py/esptool.py crosstool-NG/builds/xtensa-lx106-elf/bin/
