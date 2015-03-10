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
#include "os_type.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_config.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "default.html"

#define AP_SSID		"Homestark"
#define AP_PASSWORD	"homestark123"

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
/* Global */
char ap_mac[6]={0x00,0x00,0x00,0x00,0x00,0x00},
	 sta_mac[6]={0x00,0x00,0x00,0x00,0x00,0x00},
	 ssid_web[30],
	 pass_web[30],
	 broker_web[20];

bool lock_scan = false;

struct scan_config conf;
struct station_config sconf;
MQTT_Client mqttClient;

//WiFi access point data
typedef struct {
	char ssid[32];
	char rssi;
	char enc;
} ApData;

//Scan result
typedef struct {
	char scanInProgress;
	ApData **apData;
	int noAps;
} ScanResultData;

ScanResultData cgiWifiAps;

static struct espconn tcpC_conn;
static esp_tcp tcpC_tcp_conn;

os_event_t    user_procTaskQueue[user_procTaskQueueLen];
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

	//WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

	INFO("\r\nSystem MQTT started ...\r\n");
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

void configureAP() {
	static char ssid[33];
	static char password[33];
		
	struct softap_config apConfig;
  	wifi_softap_get_config(&apConfig);

	wifi_set_opmode(STATIONAP_MODE);

	os_memset(apConfig.password, 0, sizeof(apConfig.password));
	os_sprintf(password, "%s", AP_PASSWORD);
	os_memcpy(apConfig.password, password, os_strlen(password));

	os_memset(apConfig.ssid, 0, sizeof(apConfig.ssid));
	os_sprintf(ssid, "%s_%2x%2x%2x%2x%2x%2x", AP_SSID, ap_mac[0],ap_mac[1],ap_mac[2],ap_mac[3],ap_mac[4],ap_mac[5]);
	os_memcpy(apConfig.ssid, ssid, os_strlen(ssid));

	//apConfig.authmode = AUTH_WEP;
	//apConfig.authmode = AUTH_OPEN;
	apConfig.authmode = AUTH_WPA_WPA2_PSK;
	apConfig.channel = 4;
	apConfig.max_connection = 10;
	apConfig.ssid_hidden = 0;
	wifi_softap_set_config(&apConfig);
	wifi_softap_dhcps_start();
}

//Callback the code calls when a wlan ap scan is done. Basically stores the result in
//the cgiWifiAps struct.
static void ICACHE_FLASH_ATTR wifiScanDoneCb(void *arg, STATUS status) {
	int n;
	struct bss_info *bss_link = (struct bss_info *)arg;
	os_printf("wifiScanDoneCb %d\n", status);
	if (status!=OK) {
		wifi_station_disconnect(); //test HACK
		os_printf("nao esta ok algo");
		return;
	}


	//Clear prev ap data if needed.
	if (cgiWifiAps.apData!=NULL) {
		for (n=0; n<cgiWifiAps.noAps; n++) os_free(cgiWifiAps.apData[n]);
		os_free(cgiWifiAps.apData);
	}

	//Count amount of access points found.
	n=0;
	while (bss_link != NULL) {
		bss_link = bss_link->next.stqe_next;
		n++;
	}
	//Allocate memory for access point data
	cgiWifiAps.apData=(ApData **)os_malloc(sizeof(ApData *)*n);
	cgiWifiAps.noAps=n;

	//Copy access point data to the static struct
	n=0;
	bss_link = (struct bss_info *)arg;
	while (bss_link != NULL) {
		cgiWifiAps.apData[n]=(ApData *)os_malloc(sizeof(ApData));
		cgiWifiAps.apData[n]->rssi=bss_link->rssi;
		cgiWifiAps.apData[n]->enc=bss_link->authmode;
		strncpy(cgiWifiAps.apData[n]->ssid, (char*)bss_link->ssid, 32);

		bss_link = bss_link->next.stqe_next;
		n++;
	}
	lock_scan  = false;
	os_printf("Scan done: found %d APs\n", n);
	//We're done.
	cgiWifiAps.scanInProgress=0;

	int len;
	int i;
	char buff[1024];
    for(i=0;i<cgiWifiAps.noAps;i++)
    {
		len = os_sprintf(buff, "{\"essid\": \"%s\", \"rssi\": \"%d\", \"enc\": \"%d\"}%s\n", 
				cgiWifiAps.apData[i]->ssid, cgiWifiAps.apData[i]->rssi, 
				cgiWifiAps.apData[i]->enc, (i==cgiWifiAps.noAps-1)?"":",");
		os_printf("%s",buff);
  	}
}

static void ICACHE_FLASH_ATTR wifiStartScan() {
	int x;
	x=wifi_station_get_connect_status();
	if (x!=STATION_GOT_IP) {
		//Unit probably is trying to connect to a bogus AP. This messes up scanning. Stop that.
		os_printf("\nSTA status = %d. Disconnecting STA...\n", x);
		wifi_station_disconnect();
	}
	
    os_printf("Starting scanning...");
    if (wifi_station_scan(NULL, wifiScanDoneCb))
    {
    	lock_scan = true;
        os_printf("OK!");
        
    }
    else
    {
        os_printf("Error...");
    }
}

/*************************** TCP SERVER CODE ***************************/


static void ICACHE_FLASH_ATTR tcpC_recv_cb(void *arg, char *data, unsigned short len) {
  struct espconn *conn=(struct espconn *)arg;
  uint16_t i,j;
  for(i=0,j=0;i<strlen(data);i++)
  {
	if(*(data+i)=='D')
		if(*(data+i+1)=='=')	
		{	
			while(*(data+i+2+j)!='&')
			{
				ssid_web[j] = *(data+i+2+j);
				j++;
			}
			j=0;
		}
	if(*(data+i)=='S')
		if(*(data+i+1)=='=')	
		{
			while(*(data+i+2+j)!='&')
			{
				pass_web[j] = *(data+i+2+j);
				j++;
			}
			j=0;
		}
	if(*(data+i)=='R')
		if(*(data+i+1)=='=')	
		{
			while(j<10)
			{
				broker_web[j] = *(data+i+2+j);
				j++;
			}
			j=0;
		}
  }
  INFO("\n\nSSID Captured is:%s\n",ssid_web);
  INFO("Pass Captured is:%s\n",pass_web);
  INFO("Broker Captured is:%s\n",broker_web);
  // INFO(data);
  espconn_disconnect(conn);
}
 
static void ICACHE_FLASH_ATTR tcpC_recon_cb(void *arg, sint8 err) {
}
 
static void ICACHE_FLASH_ATTR tcpC_discon_cb(void *arg) {
}
 
static void ICACHE_FLASH_ATTR tcpC_sent_cb(void *arg) {
}

static void ICACHE_FLASH_ATTR tcpC_connected_cb(void *arg) {
  struct espconn *conn=arg;
 
  espconn_regist_recvcb  (conn, tcpC_recv_cb);
  espconn_regist_reconcb (conn, tcpC_recon_cb);
  espconn_regist_disconcb(conn, tcpC_discon_cb);
  espconn_regist_sentcb  (conn, tcpC_sent_cb);
 
  // char *transmission = "Hello, Welcome to Device Homestark:<br>MAC AP:<br>MAC STA:";
  sint8 d = espconn_sent(conn,html_page,strlen(html_page));
}

void ICACHE_FLASH_ATTR tcpC_conn_init() {
    tcpC_conn.type=ESPCONN_TCP;
    tcpC_conn.state=ESPCONN_NONE;
    tcpC_tcp_conn.local_port=80;
    tcpC_conn.proto.tcp=&tcpC_tcp_conn;
	
    espconn_regist_connectcb(&tcpC_conn, tcpC_connected_cb);
    espconn_accept(&tcpC_conn);
}

/*************************** TCP SERVER CODE ***************************/

static void ICACHE_FLASH_ATTR loop(os_event_t *events) {
    wifi_set_opmode(STATIONAP_MODE);
    //os_delay_us(1000000);
    if(!lock_scan)	wifiStartScan();
    system_os_post(user_procTaskPrio, 0, 0 );
}


void user_init(void) {
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_delay_us(1000000);

	wifi_get_macaddr(STATION_IF,sta_mac);
	wifi_get_macaddr(SOFTAP_IF,ap_mac);
	uint8_t i;

	INFO("\n\n\n\n[Device Homestark]\n");	
	INFO("AP MAC: ");
	for (i = 0; i < 6; ++i)
      INFO(" %02x:", (unsigned char) ap_mac[i]);
    INFO("\nSTA MAC: ");
	for (i = 0; i < 6; ++i)
      INFO(" %02x:", (unsigned char) sta_mac[i]);

  	INFO("\nInit. in 3 seconds...");
  	os_delay_us(1000000);
  	INFO("2 seconds...");
  	os_delay_us(1000000);
  	INFO("1 seconds...");
  	os_delay_us(1000000);

	configureAP();	//Configure AP device
	tcpMobileCfg(); //Problema do modo AP corrigido modificando o parâmetro de conexão para SOFTAP_MODE na função WIFI_Connect
	tcpC_conn_init();
	mqttStartConnection();
	system_os_task(loop, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}


