/**************************************************************
#															 
#	Project: Homestark - Device code of ESP8266   
#	Author: Anderson Ignacio da Silva						 
#   Date: 20/03/15											 
#	Target: ESP-8266							 
#	Inf.: http://www.esplatforms.blogspot.com.br 			 
#	Pending: Topic configure in WS
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
#include "driver/uart.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "driver/pwm.h"

#define INFO 			os_printf
#define PORT_WS 		80
#define AP_SSID			"Homestark %08X"
#define AP_PASSWORD		"homestark123"
#define FW_VERSION		"[Device Homestark - v1.0 2015]"
#define CONFIG_UDP_PORT 8080

#define TOPIC_MASTER	"/lights/%08X/dimmer/"
#define TOPIC_STATUS	"/lights/%08X/current/"
#define TOPIC_RGB_LIGHT	"/lights/%08X/RGB/"
 

#define LIGHT_RED       0
#define LIGHT_GREEN     1
#define LIGHT_BLUE      2

//#define LED_1(x)	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), x) 
#define LED_2(x)	GPIO_OUTPUT_SET(GPIO_ID_PIN(5), x) 


/* Global */
char ap_mac[6]={0x00,0x00,0x00,0x00,0x00,0x00},
	 sta_mac[6]={0x00,0x00,0x00,0x00,0x00,0x00},
	 ssid_web[30],
	 pass_web[30],
	 broker_web[20];

MQTT_Client 	mqttClient;
//MQTT_Client* 	mqttClient_P;

bool 			blinkS = true;
static ETSTimer StatusTimer;
static struct espconn *pUdpServer;

HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi/wifi.t8pl", cgiEspFsTemplate, tplWlan},
	{"/wifi/connect.cgi", cgiWiFiConnect, NULL},
	{"/wifi/setmode.cgi", cgiWifiSetMode, NULL},
	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};

typedef enum states
{
  FIRST_IDT,
  DATA_B,
  COMPLETE
}states_buffer;

states_buffer serial_s = FIRST_IDT;
uint8 buffer_current[4],i = 0;


LOCAL struct pwm_param pwm_rgb;
/* Global */

void MQTTwifiConnectCb(uint8_t status) {
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}

void wifiConnectCb(uint8_t status) {
	if(status == STATION_GOT_IP){
		udp_init();
		//MQTT_Connect(&mqttClient);
	} else {
		//INFO("\n\rProblem to get IP from router!");
		//MQTT_Disconnect(&mqttClient);
	}
}

void mqttConnectedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");

	uint8_t topic_1[64],topic_2[64],topic_3[64];

	os_sprintf(topic_1, TOPIC_MASTER, system_get_chip_id());
	os_sprintf(topic_2, TOPIC_STATUS, system_get_chip_id());
	os_sprintf(topic_3, TOPIC_RGB_LIGHT, system_get_chip_id());
	

	MQTT_Subscribe(client, topic_1, 0);
	MQTT_Subscribe(client, topic_2, 0);
	MQTT_Subscribe(client, topic_3, 0);
	//MQTT_Subscribe(client, "/light/config", 2);

	MQTT_Publish(client, topic_3, "127127127", sizeof("127127127"), 0, 0);
	//QTT_Publish(client, topic_2, "0.755mA", sizeof("0.755mA"), 1, 0);
    //MQTT_Publish(client, topic_2, "0.755mA", sizeof("0.755mA"), 1, 0);

	// MQTT_Publish(client, "/light/current", "0.755mA", 7, 0, 0);
	// MQTT_Publish(client, "/light/dimmer", "50", 2, 1, 0);
	// MQTT_Publish(client, "/light/config", "default", 7, 2, 0);
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
	INFO("@%s@",dataBuf);

	if(os_strstr(topicBuf,"RGB"))
	{
		uint8_t PWM_RED =   (*(dataBuf)-0x30)*100+(*(dataBuf+1)-0x30)*10+(*(dataBuf+2)-0x30),
				PWM_GREEN = (*(dataBuf+3)-0x30)*100+(*(dataBuf+4)-0x30)*10+(*(dataBuf+5)-0x30),
				PWM_BLUE =  (*(dataBuf+6)-0x30)*100+(*(dataBuf+7)-0x30)*10+(*(dataBuf+8)-0x30);		 
		RGBSetColor(PWM_RED,PWM_GREEN,PWM_BLUE);
	}
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
	MQTT_Connect(&mqttClient);

	INFO("\r\nSystem MQTT started ...\r\n");
	//InitWS();	
}

void APSettings() {
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
	char macaddr[6];
	
	struct softap_config apConfig;
  	wifi_softap_get_config(&apConfig);

	wifi_set_opmode(STATIONAP_MODE);
	
	os_memset(apConfig.password, 0, sizeof(apConfig.password));
	os_sprintf(password, "%s", AP_PASSWORD);
	os_memcpy(apConfig.password, password, os_strlen(password));

	os_memset(apConfig.ssid, 0, sizeof(apConfig.ssid));
	wifi_get_macaddr(SOFTAP_IF, macaddr);
	apConfig.ssid_len = os_sprintf(apConfig.ssid, "Homestark_%02x%02x%02x%02x%02x%02x", MAC2STR(macaddr));


	os_sprintf(ssid, AP_SSID, system_get_chip_id());
	os_memcpy(apConfig.ssid, ssid, os_strlen(ssid));

	//apConfig.authmode = AUTH_WEP;
	apConfig.authmode = AUTH_OPEN;
	//apConfig.authmode = AUTH_WPA_WPA2_PSK;
	apConfig.channel = 7;
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

	INFO("\n\n\n\n%s\n",FW_VERSION);	
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

void interruptHandler(){
	uint32 gpio_status;
	bool status_reset_button;
	gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

	if(gpio_status & BIT(13))
	{
		//clear interrupt status
		gpio_pin_intr_state_set(GPIO_ID_PIN(13), GPIO_PIN_INTR_DISABLE);
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(13));
		gpio_pin_intr_state_set(GPIO_ID_PIN(13), GPIO_PIN_INTR_POSEDGE);
		ResetToAP();
		//if(status_reset_button)
			//INFO("\nz\rPUSH BUTTON PRESSED");
		//else
		//	INFO("\n\rPUSH BUTTON NOT PRESSED");

		
	}
}

void ResetToAP(){
	//If we get here, we dont configure the flash yeat with user_config.h data, now let's save to flash
	os_memset(&ModuleSettings, 0x00, sizeof ModuleSettings);
	ModuleSettings.cfg_holder = 0x00FF55A6;
	os_sprintf(ModuleSettings.ssid, "%s", STA_SSID);
	os_sprintf(ModuleSettings.ssid, "%s", STA_SSID);
	os_sprintf(ModuleSettings.sta_pwd, "%s", STA_PASS);
	ModuleSettings.sta_type = STA_TYPE;
	os_sprintf(ModuleSettings.device_id, MQTT_CLIENT_ID, system_get_chip_id());
	os_sprintf(ModuleSettings.mqtt_host, "%s", MQTT_HOST);
	ModuleSettings.mqtt_port = MQTT_PORT;
	os_sprintf(ModuleSettings.mqtt_user, "%s", MQTT_USER);
	os_sprintf(ModuleSettings.mqtt_pass, "%s", MQTT_PASS);
	ModuleSettings.security = DEFAULT_SECURITY;	/* default non ssl */
	ModuleSettings.mqtt_keepalive = MQTT_KEEPALIVE;
	os_sprintf(ModuleSettings.configured, "%s", "RST");

	WriteFlash();

	system_restart();
}

void ConfigureGPIO() {
	//PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);

	//Setting interrupt in pin GPIO13 - Button Reset AP
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	PIN_PULLDWN_EN(PERIPHS_IO_MUX_MTCK_U);

	ETS_GPIO_INTR_ATTACH(interruptHandler, NULL);
	ETS_GPIO_INTR_DISABLE();
	GPIO_DIS_OUTPUT(GPIO_ID_PIN(13));
	gpio_register_set(GPIO_ID_PIN(13), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
                    | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
                    | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(13));
  	gpio_pin_intr_state_set(GPIO_ID_PIN(13), GPIO_PIN_INTR_POSEDGE);
  	ETS_GPIO_INTR_ENABLE();
}

void BlinkStatusCB() {
	if(os_strstr(ModuleSettings.configured,"OK")) {
		//LED_1(1);
		LED_2(1);
		// if(blinkS){
		// 	LED_1(1);
		// 	LED_2(0);
		// 	blinkS = false;
		// }
		// else{
		// 	LED_1(0);
		// 	LED_2(1);
		// 	blinkS = true;
		// }
	}
	else
		if(blinkS){
			//LED_1(1);
			LED_2(0);
			blinkS = false;
		}
		else{
			//LED_1(0);
			LED_2(1);
			blinkS = true;
		}
  	os_timer_disarm(&StatusTimer);
	os_timer_setfn(&StatusTimer, BlinkStatusCB, NULL);
	os_timer_arm(&StatusTimer, 500, 0);
}

void SendMQTT() {

  uint8_t topic_1[64],topic_2[64];

  os_sprintf(topic_1, TOPIC_MASTER, system_get_chip_id());
  os_sprintf(topic_2, TOPIC_STATUS, system_get_chip_id());

  i = 0;
  MQTT_Publish(&mqttClient, topic_2, buffer_current, sizeof(buffer_current), 1, 0);
  serial_s = FIRST_IDT;
}

void RecChar(uint8 CharBuffer) {
  switch(serial_s)
  {
    case FIRST_IDT:
      if(CharBuffer == '#')
        serial_s = DATA_B;
    break;
    case DATA_B:
        if(CharBuffer != '#' && i < 4)
        {
          buffer_current[i] = CharBuffer;
          i++;
        }
        else
          serial_s = COMPLETE; 
    break;
  }

  if(serial_s == COMPLETE)    SendMQTT();

}

LOCAL void ICACHE_FLASH_ATTR
udpserver_recv(void *arg, char *data, unsigned short length) {
	uint8_t i=0, j=0, ip_broker[15];

	if(*(data+0) == 'B' && 
	   *(data+1) == 'r' &&
	   *(data+2) == 'o' && 
	   *(data+3) == 'k' &&
	   *(data+4) == 'e' &&
	   *(data+5) == 'r' &&
	   *(data+6) == ':'
	   )
	{
		INFO("\n\rIP Address received from Broker [MQTT]\n\r");
		while(i<15){  
			ip_broker[i] = *(data+7+i);
			i++;
		}
		os_sprintf(ModuleSettings.mqtt_host, "%s", ip_broker);
		os_sprintf(ModuleSettings.configured, "%s", "OK");
		WriteFlash();
		mqttStartConnection();
	}
	
    INFO("\n\nData received via UDP:%s",data);
}

void ICACHE_FLASH_ATTR
udp_init(void) {    // allocate memory for the control block
    
    pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));

    // fill memory with 0 bytes
    ets_memset( pUdpServer, 0, sizeof( struct espconn ) );

    // create the connection from block
    espconn_create( pUdpServer );

    // set connection to UDP
    pUdpServer->type = ESPCONN_UDP;

    // alloc memory for the udp object in the server context and assign port
    pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
    pUdpServer->proto.udp->local_port = CONFIG_UDP_PORT;

    // register the callback function for receiving data
    espconn_regist_recvcb(pUdpServer, udpserver_recv);

    espconn_create( pUdpServer );
    // if( espconn_create( pUdpServer ) )
    // {
    //     //while(1);
    //     INFO("\n\nUDP Server created!");
    // }
    // else
    // 	INFO("\n\nProblem to create udp server!");
}

void RGBStripInit(){
	pwm_rgb.freq = 2000;
	pwm_rgb.duty[LIGHT_RED] = 127;
	pwm_rgb.duty[LIGHT_GREEN] = 127;
	pwm_rgb.duty[LIGHT_BLUE] = 127;
	pwm_init(pwm_rgb.freq, pwm_rgb.duty);
	pwm_start();

	// uint8_t pwm_1=0,pwm_2=255,pwm_3=0,teste;
	// for(teste;teste<255;teste++)
	// {
	// 	pwm_1++;
	// 	pwm_2--;
	// 	pwm_3++;
	// 	pwm_set_duty(pwm_1, LIGHT_RED);
	// 	pwm_set_duty(pwm_2, LIGHT_GREEN);
	// 	pwm_set_duty(pwm_3, LIGHT_BLUE);
	// 	pwm_start();	
	// 	os_delay_us(100000);
	// }
}

void RGBSetColor(uint8_t RED_L,uint8_t GREEN_L,uint8_t BLUE_L) {
	pwm_set_duty(RED_L, LIGHT_RED);
	pwm_set_duty(GREEN_L, LIGHT_GREEN);
	pwm_set_duty(BLUE_L, LIGHT_BLUE);
	pwm_start();
	INFO("\n\n\r[RGB Leds Color]:\n\r");
	INFO("RED=%d\n\r",RED_L);
	INFO("GREEN=%d\n\r",GREEN_L);
	INFO("BLUE=%d\n\r",BLUE_L);
}

void user_init(void) {
	uart_init(BIT_RATE_115200,BIT_RATE_115200);//stdoutInit();

	os_delay_us(1000000);

	ShowDevice();
  	ConfigureGPIO();
  	RestoreFlash();
  	SetAP(false);
 	
 	RGBStripInit();

  	os_timer_disarm(&StatusTimer);
	os_timer_setfn(&StatusTimer, BlinkStatusCB, NULL);
	os_timer_arm(&StatusTimer, 500, 0);
	
  	if(os_strstr(ModuleSettings.configured,"NO")){ //Almost configured, now we connect to wifi and wait for broadcast UDP
  		WIFI_Connect(ModuleSettings.ssid, ModuleSettings.sta_pwd, wifiConnectCb); //mqttStartConnection();
  	}
  	else if(os_strstr(ModuleSettings.configured,"OK")){ //Network configured, let's connect to MQTT Broker
		WIFI_Connect(ModuleSettings.ssid, ModuleSettings.sta_pwd, MQTTwifiConnectCb); //mqttStartConnection();
  		mqttStartConnection();
  	}
  	else //Nothing configured, just starting for first cfg
  		InitWS();
}


