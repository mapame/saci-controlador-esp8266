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
#include "rtc.h"
#include "configuration.h"
#include "module_manager.h"
#include "comm.h"


void telnet_task(void *pvParameters);
void httpd_task(void *pvParameters);
void mqtt_task(void *pvParameters);
void module_manager_task(void *pvParameters);
void custom_code_task(void *pvParameters);


int ap_mode = 0;

int config_diagnostic_mode;
char config_wifi_ssid[CONFIG_STR_SIZE];
char config_wifi_password[CONFIG_STR_SIZE];
char config_wifi_ap_password[CONFIG_STR_SIZE];

int config_mqtt_enabled;

TaskHandle_t module_manager_task_handle = NULL;
TaskHandle_t custom_code_task_handle = NULL;
TaskHandle_t httpd_task_handle = NULL;
TaskHandle_t mqtt_task_handle = NULL;

void IRAM blink_task(void *pvParameters) {
	int cycle = 0;
	
	while(1){
		if(ap_mode) {
			gpio_write(LED_G_PIN, cycle % 2);
			gpio_write(LED_R_PIN, (cycle + 1) % 2);
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
	
	comm_init();
	
	gpio_enable(LED_R_PIN, GPIO_OUTPUT);
	gpio_enable(LED_G_PIN, GPIO_OUTPUT);
	gpio_enable(BTN_PIN, GPIO_INPUT);
	
	gpio_set_pullup(BTN_PIN, 1, 0);
	
	gpio_write(LED_R_PIN, 0);
	gpio_write(LED_G_PIN, 0);
	
	if(i2c_init(0, SCL_PIN, SDA_PIN, I2C_FREQ_400K))
		debug("Failed to initialize i2c bus!\n");
	
	rtc_init();
	
	debug("Firmware version: "FW_VERSION"\n");
	debug("Custom code version: %s\n", custom_code_version);
	debug("Build date: "__DATE__" "__TIME__"\n");
	
	vTaskDelay(pdMS_TO_TICKS(100));
	
	for(int i = 0; i < 20; i++) {
		gpio_write(LED_R_PIN, i % 2);
		gpio_write(LED_G_PIN, (i + 1) % 2);
		
		button += (gpio_read(BTN_PIN) ? 0 : 1);
		
		vTaskDelay(pdMS_TO_TICKS(100));
	}
	
	gpio_write(LED_R_PIN, 0);
	gpio_write(LED_G_PIN, 0);
	
	if(button > 8) {
		while(!gpio_read(BTN_PIN)) {
			vTaskDelay(pdMS_TO_TICKS(1000));
			
			if(button++ == 50) {
				
				sysparam_set_data("wifi_password", NULL, 0, 0);
				sysparam_set_data("wifi_ap_password", NULL, 0, 0);
				sysparam_set_data("webui_password", NULL, 0, 0);
				
				gpio_write(LED_R_PIN, 1);
				gpio_write(LED_G_PIN, 1);
			}
		}
	}
	
	configuration_cleanup();
	configuration_load();
	
	if(button > 8 || strlen(config_wifi_ssid) < 1 || strlen(config_wifi_password) < 1) {
		struct sdk_softap_config ap_config;
		struct ip_info ap_ip;
		ip4_addr_t dhcp_first_ip;
		
		ap_mode = 1;
		
		sdk_wifi_set_opmode(SOFTAP_MODE);
		
		IP4_ADDR(&ap_ip.ip, 10, 0, 0, 1);
		IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
		IP4_ADDR(&ap_ip.netmask, 255, 255, 0, 0);
		
		sdk_wifi_set_ip_info(1, &ap_ip);
		
		strcpy((char *)ap_config.ssid, WIFI_AP_SSID);
		strcpy((char *)ap_config.password, config_wifi_ap_password);
		
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
	
	xTaskCreate(&module_manager_task, "module_manager_task", 512, NULL, 5, &module_manager_task_handle);
	xTaskCreate(&custom_code_task, "custom_code_task", 512, NULL, 4, &custom_code_task_handle);
	xTaskCreate(&httpd_task, "http_task", 1024, NULL, 3, &httpd_task_handle);
	xTaskCreate(&blink_task, "blink_task", 256, NULL, 1, NULL);
	
	if(ap_mode)
		xTaskCreate(&telnet_task, "telnet_task", 512, NULL, 2, NULL);
	else if(config_mqtt_enabled)
		xTaskCreate(&mqtt_task, "mqtt_task", 1024+512, NULL, 2, &mqtt_task_handle);
}
