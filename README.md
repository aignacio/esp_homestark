<img src="http://s27.postimg.org/iflbj5w4z/logo_index.jpg" height="45" width="150">
=================

ESP8266 - Firmware

This is fork mixed of [esp_mqtt](https://github.com/tuanpmt/esp_mqtt) and [esphttpd](http://git.spritesserver.nl/esphttpd.git/). 
The library for compressing the files is a submodule then you need to add with:
  
    cd esp_homestark
    git submodule init
    git submodule update

When you program the ESP8266, you need to connect to the module and then access his [ip address](192.168.4.1),
when you open the page, the wi-fi ap's will be listed to you connect. After connected, insert in specific box of HTML's webpage, the <b>BROKER IP</b> of MQTT Network.

You still can use a websocket in python to connect to your's localhost broker like [mosquitto](http://mosquitto.org/):
These tutorial's can help you in this, but you need to compile by your own pc at the moment:

* [Websocket Client js](http://jpmens.net/2014/07/03/the-mosquitto-mqtt-broker-gets-websockets-support/)
* [Building Mosquitto with support for websocket](https://goochgooch.wordpress.com/2014/08/01/building-mosquitto-1-4/)

![ScreenShot](http://s17.postimg.org/k4y9ohc4v/teste.jpg)

__Requirements:__
 
* [SDK Espressif and tools](https://github.com/esp8266/esp8266-wiki/wiki/Toolchain)

__Instructions:__

To compile and flash is simple, just follow these steps:

    cd esp_homestark
    make clean
    make all

Flash:

    make flash
    make test
