/**************************************************************
#															 
#	Project: Homestark - Device code of ESP8266   
#	Author: Anderson Ignacio da Silva						 
#   Date: 13/03/15											 
#	Target: ESP-8266							 
#	Inf.: http://www.esplatforms.blogspot.com.br 			 
#	Pending: Communication Serial and topic configure in WS
#	(WebServer)		
#													 
**************************************************************/

#include "config.h"
#include "espmissingincludes.h"
#include "ets_sys.h"
#include "osapi.h"
#include "httpd.h"
#include "io.h"
#include "httpdespfs.h"
#include "cgi.h"
#include "cgiwifi.h"
#include "stdout.h"
#include "auth.h"
#include "wifi.h"
#include "mqtt.h"
#include "mem.h"
#include "gpio.h"
#include "debug.h"
#include "user_interface.h"
#include "uart.h"


#define INFO 					os_printf
#define PORT_WS 				80
#define AP_SSID					"Homestark"
#define AP_PASSWORD				"homestark123"

/* Global */
char ap_mac[6]={0x00,0x00,0x00,0x00,0x00,0x00},
	 sta_mac[6]={0x00,0x00,0x00,0x00,0x00,0x00},
	 ssid_web[30],
	 pass_web[30],
	 broker_web[20];

MQTT_Client 	mqttClient;
os_event_t   	user_procTaskQueue[user_procTaskQueueLen];

HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
	{"/wifi/connect.cgi", cgiWiFiConnect, NULL},
	{"/wifi/setmode.cgi", cgiWifiSetMode, NULL},
	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};

/* Global */

void wifiConnectCb(uint8_t status) {
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}

void mqttConnectedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");
	MQTT_Subscribe(client, "/mqtt/topic/0", 0);
	MQTT_Subscribe(client, "/mqtt/topic/1", 1);
	MQTT_Subscribe(client, "/mqtt/topic/2", 2);

	MQTT_Publish(client, "/mqtt/topic/0", "hello0", 6, 0, 0);
	MQTT_Publish(client, "/mqtt/topic/1", "hello1", 6, 1, 0);
	MQTT_Publish(client, "/mqtt/topic/2", "hello2", 6, 2, 0);
}

void mqttDisconnectedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len) {
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

void mqttStartConnection() {
	//CFG_Load();

	MQTT_InitConnection(&mqttClient, ModuleSettings.mqtt_host, ModuleSettings.mqtt_port, ModuleSettings.security);
	//MQTT_InitConnection(&mqttClient, "192.168.11.122", 1880, 0);

	MQTT_InitClient(&mqttClient, ModuleSettings.device_id, ModuleSettings.mqtt_user, ModuleSettings.mqtt_pass, ModuleSettings.mqtt_keepalive, 1);
	//MQTT_InitClient(&mqttClient, "client_id", "user", "pass", 120, 1);

	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	//MQTT_Connect(&mqttClient);
	
	WIFI_Connect(ModuleSettings.ssid, ModuleSettings.sta_pwd, wifiConnectCb);
	
	INFO("\r\nSystem MQTT started ...\r\n");
	InitWS();	
}

void tcpMobileCfg() {
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
	
	switch(wifi_softap_dhcps_status())
	{
		case DHCP_STOPPED:
			INFO("\nDHCP Server stopped\n");
		break;
		case DHCP_STARTED:
			INFO("\nDHCP Server initialized\n");
		break;
	}

	switch(wifi_get_opmode())
	{
		case STATION_MODE:
			INFO("OM: Station");
		break;
		case SOFTAP_MODE:
			INFO("OM: Access Point Mode");

		break;
		case STATIONAP_MODE:
			INFO("OM: Access Point Mode + Station");

		break;
	}
}

void SetAP(bool ssid_hidden) {
	static char ssid[33];
	static char password[33];
		
	struct softap_config apConfig;
  	wifi_softap_get_config(&apConfig);

	wifi_set_opmode(STATIONAP_MODE);

	os_memset(apConfig.password, 0, sizeof(apConfig.password));
	os_sprintf(password, "%s", AP_PASSWORD);
	os_memcpy(apConfig.password, password, os_strlen(password));

	os_memset(apConfig.ssid, 0, sizeof(apConfig.ssid));
	os_sprintf(ssid, "%s %08X", AP_SSID, system_get_chip_id());
	os_memcpy(apConfig.ssid, ssid, os_strlen(ssid));

	//apConfig.authmode = AUTH_WEP;
	//apConfig.authmode = AUTH_OPEN;
	apConfig.authmode = AUTH_WPA_WPA2_PSK;
	apConfig.channel = 4;
	apConfig.max_connection = 10;
	if(!ssid_hidden)	
		apConfig.ssid_hidden = 0;
	else
		apConfig.ssid_hidden = 1;
	wifi_softap_set_config(&apConfig);
	wifi_softap_dhcps_start();
}

void RestoreFlash() {
	ReadFlash();
	uint8_t* pass_ss = ModuleSettings.sta_pwd;

	if(os_strstr(pass_ss,STA_PASS)) {
		INFO("\n\r@Configurations Default!");
	}
	else { 
		INFO("\n\r@Different Configurations!");
		//If we get here, we dont configure the flash yeat with user_config.h data, now let's save to flash
		//os_memset(&ModuleSettings, 0x00, sizeof ModuleSettings);
		//ModuleSettings.cfg_holder = 0x00FF55A6;
		//os_sprintf(ModuleSettings.sta_ssid_s, "%s", STA_SSID);
		//os_sprintf(ModuleSettings.ssid, "%s", STA_SSID);
		//os_sprintf(ModuleSettings.sta_pwd, "%s", STA_PASS);
		ModuleSettings.sta_type = STA_TYPE;
		os_sprintf(ModuleSettings.device_id, MQTT_CLIENT_ID, system_get_chip_id());
		//os_sprintf(ModuleSettings.mqtt_host, "%s", MQTT_HOST);
		ModuleSettings.mqtt_port = MQTT_PORT;
		os_sprintf(ModuleSettings.mqtt_user, "%s", MQTT_USER);
		os_sprintf(ModuleSettings.mqtt_pass, "%s", MQTT_PASS);
		ModuleSettings.security = DEFAULT_SECURITY;	/* default non ssl */
		//ModuleSettings.mqtt_keepalive = MQTT_KEEPALIVE;
		WriteFlash();
	}
}

void ShowDevice() {
	uint8_t i;
	wifi_get_macaddr(STATION_IF,sta_mac);
	wifi_get_macaddr(SOFTAP_IF,ap_mac);

	INFO("\n\n\n\n[Device Homestark]\n");	
	INFO("\n\r\tAP MAC: ");
	for (i = 0; i < 6; ++i)
      INFO(" %02x:", (unsigned char) ap_mac[i]);
    INFO("\n\r\tSTA MAC: ");
	for (i = 0; i < 6; ++i)
      INFO(" %02x:", (unsigned char) sta_mac[i]);

  	INFO("\nInit. in 3...");
  	os_delay_us(500000);
  	INFO("2...");
  	os_delay_us(500000);
  	INFO("1...\n\r");
  	os_delay_us(500000);
}

void InitWS() {
	httpdInit(builtInUrls, PORT_WS);
	os_printf("\nHttpd Web server at PORT:%d - Ready\n\r",PORT_WS);
}

void user_init(void) {
	stdoutInit();
	os_delay_us(1000000);

	ShowDevice();
  	RestoreFlash();
  	SetAP(false);
 
  	if(ModuleSettings.mqtt_keepalive == 125)
  		mqttStartConnection();
  	else
  		InitWS();
}


