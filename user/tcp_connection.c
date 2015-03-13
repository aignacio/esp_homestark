// static struct espconn tcpC_conn;
//static esp_tcp tcpC_tcp_conn;
///*************************** TCP SERVER CODE ***************************/
// static void ICACHE_FLASH_ATTR tcpC_recv_cb(void *arg, char *data, unsigned short len) {
//   struct espconn *conn=(struct espconn *)arg;
//   uint16_t i,j;
  
//   //   GET or POST ?/SSID=XXXX &PASS=XXXX & &BROKER= xxxxxx
//   for(i=0,j=0;i<strlen(data);i++)
//   {
// 	if(*(data+i)=='D')
// 		if(*(data+i+1)=='=')	
// 		{	
// 			while(*(data+i+2+j)!='&')
// 			{
// 				ssid_web[j] = *(data+i+2+j);
// 				j++;
// 			}
// 			j=0;
// 		}
// 	if(*(data+i)=='S')
// 		if(*(data+i+1)=='=')	
// 		{
// 			while(*(data+i+2+j)!='&')
// 			{
// 				pass_web[j] = *(data+i+2+j);
// 				j++;
// 			}
// 			j=0;
// 		}
// 	if(*(data+i)=='R')
// 		if(*(data+i+1)=='=')	
// 		{
// 			while(j<10)
// 			{
// 				broker_web[j] = *(data+i+2+j);
// 				j++;
// 			}
// 			j=0;
// 		}
//   }
//   INFO("\n\nSSID Captured is:%s\n",ssid_web);
//   INFO("Pass Captured is:%s\n",pass_web);
//   INFO("Broker Captured is:%s\n",broker_web);
//   // INFO(data);
//   espconn_disconnect(conn);
// }
 
// static void ICACHE_FLASH_ATTR tcpC_recon_cb(void *arg, sint8 err) {
// }
 
// static void ICACHE_FLASH_ATTR tcpC_discon_cb(void *arg) {
// }
 
// static void ICACHE_FLASH_ATTR tcpC_sent_cb(void *arg) {
// }

// static void ICACHE_FLASH_ATTR tcpC_connected_cb(void *arg) {
//   struct espconn *conn=arg;
 
//   espconn_regist_recvcb  (conn, tcpC_recv_cb);
//   espconn_regist_reconcb (conn, tcpC_recon_cb);
//   espconn_regist_disconcb(conn, tcpC_discon_cb);
//   espconn_regist_sentcb  (conn, tcpC_sent_cb);
 
//   // char *transmission = "Hello, Welcome to Device Homestark:<br>MAC AP:<br>MAC STA:";
//   sint8 d = espconn_sent(conn,html_page,strlen(html_page));
// }

// void ICACHE_FLASH_ATTR tcpC_conn_init() {
//     tcpC_conn.type=ESPCONN_TCP;
//     tcpC_conn.state=ESPCONN_NONE;
//     tcpC_tcp_conn.local_port=80;
//     tcpC_conn.proto.tcp=&tcpC_tcp_conn;
	
//     espconn_regist_connectcb(&tcpC_conn, tcpC_connected_cb);
//     espconn_accept(&tcpC_conn);
// }

// /*************************** TCP SERVER CODE ***************************/

// static void ICACHE_FLASH_ATTR wifiStartScan() {
// 	int x;
// 	x=wifi_station_get_connect_status();
// 	if (x!=STATION_GOT_IP) {
// 		//Unit probably is trying to connect to a bogus AP. This messes up scanning. Stop that.
// 		os_printf("\nSTA status = %d. Disconnecting STA...\n", x);
// 		wifi_station_disconnect();
// 	}
	
//     os_printf("Starting scanning...");
//     if (wifi_station_scan(NULL, wifiScanDoneCb))
//     {
//     	lock_scan = true;
//         os_printf("OK!");
        
//     }
//     else
//     {
//         os_printf("Error...");
//     }
// }

// //Callback the code calls when a wlan ap scan is done. Basically stores the result in
// //the cgiWifiAps struct.
// static void ICACHE_FLASH_ATTR wifiScanDoneCb(void *arg, STATUS status) {
// 	int n;
// 	struct bss_info *bss_link = (struct bss_info *)arg;
// 	os_printf("wifiScanDoneCb %d\n", status);
// 	if (status!=OK) {
// 		wifi_station_disconnect(); //test HACK
// 		os_printf("nao esta ok algo");
// 		return;
// 	}


// 	//Clear prev ap data if needed.
// 	if (cgiWifiAps.apData!=NULL) {
// 		for (n=0; n<cgiWifiAps.noAps; n++) os_free(cgiWifiAps.apData[n]);
// 		os_free(cgiWifiAps.apData);
// 	}

// 	//Count amount of access points found.
// 	n=0;
// 	while (bss_link != NULL) {
// 		bss_link = bss_link->next.stqe_next;
// 		n++;
// 	}
// 	//Allocate memory for access point data
// 	cgiWifiAps.apData=(ApData **)os_malloc(sizeof(ApData *)*n);
// 	cgiWifiAps.noAps=n;

// 	//Copy access point data to the static struct
// 	n=0;
// 	bss_link = (struct bss_info *)arg;
// 	while (bss_link != NULL) {
// 		cgiWifiAps.apData[n]=(ApData *)os_malloc(sizeof(ApData));
// 		cgiWifiAps.apData[n]->rssi=bss_link->rssi;
// 		cgiWifiAps.apData[n]->enc=bss_link->authmode;
// 		strncpy(cgiWifiAps.apData[n]->ssid, (char*)bss_link->ssid, 32);

// 		bss_link = bss_link->next.stqe_next;
// 		n++;
// 	}
// 	lock_scan  = false;
// 	os_printf("Scan done: found %d APs\n", n);
// 	//We're done.
// 	cgiWifiAps.scanInProgress=0;

// 	int len;
// 	int i;
// 	char buff[1024];
//     for(i=0;i<cgiWifiAps.noAps;i++)
//     {
// 		len = os_sprintf(buff, "{\"essid\": \"%s\", \"rssi\": \"%d\", \"enc\": \"%d\"}%s\n", 
// 				cgiWifiAps.apData[i]->ssid, cgiWifiAps.apData[i]->rssi, 
// 				cgiWifiAps.apData[i]->enc, (i==cgiWifiAps.noAps-1)?"":",");
// 		os_printf("%s",buff);
//   	}
// }



