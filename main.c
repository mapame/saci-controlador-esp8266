#include <espressif/esp_common.h>
#include <espressif/esp_misc.h>
#include <esp8266.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <i2c/i2c.h>
#include <dhcpserver.h>
#include <sysparam.h>

#include <FreeRTOS.h>
#include <task.h>

#include "common.h"
#include "module_manager.h"
#include "comm.h"

char ap_mode = 0;

void telnet_task(void *pvParameters);
void httpd_task(void *pvParameters);
void module_manager_task(void *pvParameters);

void IRAM blink_task(void *pvParameters) {
	int cycle = 0;
	
	while(1){
		if(ap_mode) {
			gpio_write(LED_R_PIN, 1);
			gpio_write(LED_G_PIN, 1);
		} else {
			if(sdk_wifi_station_get_connect_status() == STATION_GOT_IP) {
				gpio_write(LED_G_PIN, cycle % 2);
				gpio_write(LED_R_PIN, 0);
			} else {
				gpio_write(LED_R_PIN, cycle % 2);
				gpio_write(LED_G_PIN, 0);
			}
		}
		
		cycle = (cycle + 1) % 2;
		
		vTaskDelay(pdMS_TO_TICKS(250));
	}
}

void user_init(void) {
	int button = 0;
	
	char *config_wifi_ssid = NULL;
	char *config_wifi_password = NULL;
	char *config_wifi_ap_password = NULL;
	
	comm_init();
	
	gpio_enable(LED_R_PIN, GPIO_OUTPUT);
	gpio_enable(LED_G_PIN, GPIO_OUTPUT);
	gpio_enable(BTN_PIN, GPIO_INPUT);
	
	gpio_set_pullup(BTN_PIN, 1, 0);
	
	gpio_write(LED_R_PIN, 0);
	gpio_write(LED_G_PIN, 0);
	
	if(i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_500K))
		debug("Failed to initialize i2c bus!\n");
	
	debug("Firmware version: "FW_VERSION"\n");
	debug("Build date: "__DATE__" "__TIME__"\n");
	
	vTaskDelay(pdMS_TO_TICKS(100));
	sysparam_get_string("wifi_ssid", &config_wifi_ssid);
	sysparam_get_string("wifi_password", &config_wifi_password);
	sysparam_get_string("wifi_ap_password", &config_wifi_ap_password);
	
	for(int i = 0; i < 8; i++) {
		gpio_write(LED_R_PIN, i % 2);
		gpio_write(LED_G_PIN, i % 2);
		
		button += (gpio_read(BTN_PIN) ? 0 : 1);
		
		vTaskDelay(pdMS_TO_TICKS(250));
	}
	
	gpio_write(LED_R_PIN, 0);
	gpio_write(LED_G_PIN, 0);
	
	if(button > 5 || !config_wifi_ssid || !config_wifi_password || strlen(config_wifi_ssid) < 1 || strlen(config_wifi_password) < 8) {
		struct sdk_softap_config ap_config;
		struct ip_info ap_ip;
		ip4_addr_t dhcp_first_ip;
		
		gpio_write(LED_R_PIN, 1);
		gpio_write(LED_G_PIN, 1);
		
		ap_mode = 1;
		
		sdk_wifi_set_opmode(SOFTAP_MODE);
		
		IP4_ADDR(&ap_ip.ip, 10, 0, 0, 1);
		IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
		IP4_ADDR(&ap_ip.netmask, 255, 255, 0, 0);
		
		sdk_wifi_set_ip_info(1, &ap_ip);
		
		strcpy((char *)ap_config.ssid, WIFI_AP_SSID);
		
		if(config_wifi_ap_password && strlen(config_wifi_ap_password) > 7)
			strcpy((char *)ap_config.password, config_wifi_ap_password);
		else
			strcpy((char *)ap_config.password, WIFI_AP_DEFAULT_PASSWORD);
		
		ap_config.ssid_len = strlen(WIFI_AP_SSID);
		ap_config.ssid_hidden = 0;
		ap_config.channel = 1;
		ap_config.authmode = AUTH_WPA_WPA2_PSK;
		ap_config.max_connection = 2;
		ap_config.beacon_interval = 100;
		
		sdk_wifi_softap_set_config(&ap_config);
		
		dhcp_first_ip.addr = ap_ip.ip.addr + htonl(1);
		
		dhcpserver_start(&dhcp_first_ip, 3);
		dhcpserver_set_router(&ap_ip.ip);
	} else {
		struct sdk_station_config wifi_config;
		
		strcpy((char *)wifi_config.ssid, config_wifi_ssid);
		strcpy((char *)wifi_config.password, config_wifi_password);
		
		sdk_wifi_set_opmode(STATION_MODE);
		sdk_wifi_station_set_config(&wifi_config);
	}
	
	xTaskCreate(&module_manager_task, "module_manager_task", 512, NULL, 4, NULL);
	xTaskCreate(&telnet_task, "telnet_task", 512, NULL, 3, NULL);
	xTaskCreate(&httpd_task, "http_task", 512, NULL, 2, NULL);
	xTaskCreate(&blink_task, "blink_task", 256, NULL, 1, NULL);
}
