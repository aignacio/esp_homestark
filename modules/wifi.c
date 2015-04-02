/*
 * wifi.c
 *
 *  Created on: Dec 30, 2014
 *      Author: Minh
 */
#include "wifi.h"
#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "mqtt_msg.h"
#include "debug.h"
#include "user_config.h"
#include "config.h"

#define TRY_AP                   10
#define TIME_TIMEOUT_AP			 1000	

bool enableTimeoutTimerAP = 1;
uint8_t TryConnectAP = TRY_AP;
os_timer_t timeout_timer_ap;

static ETSTimer WiFiLinker;
WifiCallback wifiCb = NULL;
static uint8_t wifiStatus = STATION_IDLE, lastWifiStatus = STATION_IDLE;
static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
	struct ip_info ipConfig;

	os_timer_disarm(&WiFiLinker);
	wifi_get_ip_info(STATION_IF, &ipConfig);
	wifiStatus = wifi_station_get_connect_status();
	if (wifiStatus == STATION_GOT_IP && ipConfig.ip.addr != 0)
	{

		os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
		os_timer_arm(&WiFiLinker, 2000, 0);
		enableTimeoutTimerAP = 0;
		os_timer_disarm(&timeout_timer_ap);

	}
	else
	{
		if(wifi_station_get_connect_status() == STATION_WRONG_PASSWORD)
		{

			INFO("STATION_WRONG_PASSWORD\r\n");
			wifi_station_connect();


		}
		else if(wifi_station_get_connect_status() == STATION_NO_AP_FOUND)
		{

			INFO("STATION_NO_AP_FOUND\r\n");
			wifi_station_connect();


		}
		else if(wifi_station_get_connect_status() == STATION_CONNECT_FAIL)
		{

			INFO("STATION_CONNECT_FAIL\r\n");
			wifi_station_connect();

		}
		else
		{
			INFO("STATION_IDLE\r\n");
		}

		os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
		os_timer_arm(&WiFiLinker, 500, 0);
	}
	if(wifiStatus != lastWifiStatus){
		lastWifiStatus = wifiStatus;
		if(wifiCb)
			wifiCb(wifiStatus);
	}
}

void timeout_timerWifiCb(){
	if(enableTimeoutTimerAP){
		INFO("\n\r[Timeout]Try number:%d\n\n\r",TRY_AP-TryConnectAP);
		TryConnectAP--;
		if(!TryConnectAP)
			ResetToAP();
		os_timer_disarm(&timeout_timer_ap);
	    os_timer_setfn(&timeout_timer_ap, (os_timer_func_t *)timeout_timerWifiCb, NULL);
	    os_timer_arm(&timeout_timer_ap, TIME_TIMEOUT_AP, 0);
	}
}

void EnableTimeout(){
	INFO("\n\n\rTimeout Timer Enabled!\n\r");
	os_timer_disarm(&timeout_timer_ap);
    os_timer_setfn(&timeout_timer_ap, (os_timer_func_t *)timeout_timerWifiCb, NULL);
    os_timer_arm(&timeout_timer_ap, TIME_TIMEOUT_AP, 0);
}


void ICACHE_FLASH_ATTR WIFI_Connect(uint8_t* ssid, uint8_t* pass, WifiCallback cb)
{
	struct station_config stationConf;

	INFO("\n\rTry to connect to AP\n\r");
	wifi_set_opmode(STATION_MODE);
	wifi_station_set_auto_connect(FALSE);
	wifiCb = cb;

	os_memset(&stationConf, 0, sizeof(struct station_config));

	os_sprintf(stationConf.ssid, "%s", ssid);
	os_sprintf(stationConf.password, "%s", pass);

	wifi_station_set_config(&stationConf);

	os_timer_disarm(&WiFiLinker);
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, 1000, 0);

	wifi_station_set_auto_connect(TRUE);
	wifi_station_connect();
	EnableTimeout();
}

