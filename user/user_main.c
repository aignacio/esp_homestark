/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"

#define WIFI_APPASSWORD	"3102230u"
#define WIFI_APSSID		"Homestark"
MQTT_Client mqttClient;

void wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}
void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");
	MQTT_Subscribe(client, "/mqtt/topic/0", 0);
	MQTT_Subscribe(client, "/mqtt/topic/1", 1);
	MQTT_Subscribe(client, "/mqtt/topic/2", 2);

	MQTT_Publish(client, "/mqtt/topic/0", "hello0", 6, 0, 0);
	MQTT_Publish(client, "/mqtt/topic/1", "hello1", 6, 1, 0);
	MQTT_Publish(client, "/mqtt/topic/2", "hello2", 6, 2, 0);

}

void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len+1),
			*dataBuf = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
	os_free(topicBuf);
	os_free(dataBuf);
}

void mqttStartConnection()
{
	CFG_Load();

	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
	//MQTT_InitConnection(&mqttClient, "192.168.11.122", 1880, 0);

	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
	//MQTT_InitClient(&mqttClient, "client_id", "user", "pass", 120, 1);

	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

	INFO("\r\nSystem MQTT started ...\r\n");
}

void tcpMobileCfg()
{
	/****** Print Some inf. about ESP ******/
	os_printf("\n\n");
	os_printf("\nPrinting AP Settings:\n");
	struct softap_config cfgESP;
	while(!wifi_softap_get_config(&cfgESP));
	os_printf("\nSSID:%s",cfgESP.ssid);
	os_printf("\nPASSWORD:%s",cfgESP.password);
	os_printf("\nCHANNEL:%d",cfgESP.channel);
	os_printf("\nAuthentification:%d",cfgESP.authmode);
	os_printf("\nSSID Hidden:%d",cfgESP.ssid_hidden);
	os_printf("\nMax Connections:%d",cfgESP.max_connection);
	os_printf("\n\n");
	//wifi_set_opmode(0x3);
	while(1);
}

void configureAP()
{
	static char ssid[33];
	static char password[33];
	static uint8_t macaddr[6];
	
	struct softap_config apConfig;
	wifi_set_opmode(0x3);
	switch(wifi_get_opmode())
	{
		case STATION_MODE:
			INFO("Modo de operação: Station");
		break;
		case SOFTAP_MODE:
			INFO("Modo de operação: Access Point Mode");

		break;
		case STATIONAP_MODE:
			INFO("Modo de operação: Access Point Mode + Station");

		break;
	}

	os_memset(apConfig.password, 0, sizeof(apConfig.password));
	os_sprintf(password, "%s", WIFI_APPASSWORD);
	os_memcpy(apConfig.password, password, os_strlen(password));

	os_memset(apConfig.ssid, 0, sizeof(apConfig.ssid));
	os_sprintf(ssid, "%s", WIFI_APSSID);
	os_memcpy(apConfig.ssid, ssid, os_strlen(ssid));

	apConfig.authmode = AUTH_WEP;
	//apConfig.authmode = AUTH_WPA_WPA2_PSK;
	//apConfig.authmode = AUTH_OPEN;
	apConfig.channel = 7;
	apConfig.max_connection = 10;
	apConfig.ssid_hidden = 0;
	wifi_softap_set_config(&apConfig);

	wifi_softap_dhcps_start();
	switch(wifi_softap_dhcps_status())
	{
		case DHCP_STOPPED:
			INFO("\nDHCP Server parou!\n");
		break;
		case DHCP_STARTED:
			INFO("\nDHCP Server foi iniciado!\n");
		break;
	}

}

void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_delay_us(1000000);

	configureAP();
	tcpMobileCfg();
	//mqttStartConnection();
}
