#include "espressif/esp_common.h"

#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <mqtt_bearssl.h>

#include "common.h"
#include "configuration.h"
#include "rtc.h"

#include "tls_tas.h"


typedef struct mqtt_subscription_s {
	char topic_name[64];
	int max_qos;
	QueueHandle_t *queue;
} mqtt_subscription_t;


extern QueueHandle_t cc_command_queue;

const mqtt_subscription_t sub_list[] = {
	{"comandos", 0, &cc_command_queue},
	{"", 0, NULL}
};

char config_mqtt_hostname[CONFIG_STR_SIZE];
char config_mqtt_port[CONFIG_STR_SIZE];
char config_mqtt_username[CONFIG_STR_SIZE];
char config_mqtt_password[CONFIG_STR_SIZE];
char config_mqtt_clientid[CONFIG_STR_SIZE];
char config_mqtt_topic_prefix[CONFIG_STR_SIZE];

bearssl_context ctx;
struct mqtt_client client = {.error = MQTT_ERROR_CONNECT_NOT_CALLED};

uint8_t mqtt_sendbuf[1024];
uint8_t mqtt_recvbuf[1024];

float mqtt_cycle_duration;

int mqtt_task_publish_text(const char* topic, const char* text, int qos, int retain) {
	char full_topic_name[256];
	uint8_t publish_flags = 0;
	
	if(topic == NULL || text == NULL)
		return -1;
	
	if(client.error != MQTT_OK)
		return -2;
	
	if(retain)
		publish_flags |= MQTT_PUBLISH_RETAIN;
	
	if(qos == 1)
		publish_flags |= MQTT_PUBLISH_QOS_1;
	else if(qos == 2)
		publish_flags |= MQTT_PUBLISH_QOS_2;
	else
		publish_flags |= MQTT_PUBLISH_QOS_0;
	
	snprintf(full_topic_name, sizeof(full_topic_name), "%s%s", config_mqtt_topic_prefix, topic);
	
	mqtt_publish(&client, full_topic_name, text, strlen(text) + 1, publish_flags);
	
	/*
	if(client.error != MQTT_OK) {
		printf("MQTT Error: %s\n", mqtt_error_str(client.error));
	}
	*/
	
	return (client.error == MQTT_OK) ? 0 : -3;
}

int mqtt_task_publish_int(const char* topic, int value, int qos, int retain) {
	char aux[32];
	
	snprintf(aux, sizeof(aux), "%d", value);
	return mqtt_task_publish_text(topic, aux, qos, retain);
}

int mqtt_task_publish_float(const char* topic, float value, int qos, int retain) {
	char aux[32];
	
	snprintf(aux, sizeof(aux), "%.5f", value);
	return mqtt_task_publish_text(topic, aux, qos, retain);
}

static void sub_callback(void** unused, struct mqtt_response_publish *received) {
	char full_topic_name[256];
	
	for(int i = 0; sub_list[i].queue != NULL; i++) {
		snprintf(full_topic_name, sizeof(full_topic_name), "%s%s", config_mqtt_topic_prefix, sub_list[i].topic_name);
		
		if(!strncmp((char*)received->topic_name, full_topic_name, received->topic_name_size)) {
			xQueueSend(*(sub_list[i].queue), received->application_message, 0);
			break;
		}
	}
}

void mqtt_task(void *pvParameters) {
	uint8_t mqtt_connect_flags = 0;
	time_t rtc_time;
	char full_topic_name[256];
	int rc;
	
	uint32_t start_time, end_time;
	int cycle_duration[3], cycle_count = 0;
	
	
	brssl_mqtt_init(&ctx, 512, TAs, TAs_NUM);
	
	if(strlen(config_mqtt_clientid) == 0) {
		mqtt_connect_flags |= MQTT_CONNECT_CLEAN_SESSION;
	}
	
	while(1) {
		if(sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
			vTaskDelay(pdMS_TO_TICKS(500));
			continue;
		}
		
		rtc_get_time(&rtc_time);
		
		if(brssl_mqtt_connect(&ctx, config_mqtt_hostname, config_mqtt_port, rtc_time)!= 0) {
			vTaskDelay(pdMS_TO_TICKS(500));
			continue;
		}
		
		mqtt_init(&client, &ctx, mqtt_sendbuf, sizeof(mqtt_sendbuf), mqtt_recvbuf, sizeof(mqtt_recvbuf), sub_callback);
		mqtt_connect(&client, config_mqtt_clientid, NULL, NULL, 0, config_mqtt_username, config_mqtt_password, mqtt_connect_flags, 300);
		
		for(int i = 0; sub_list[i].queue != NULL; i++) {
			snprintf(full_topic_name, sizeof(full_topic_name), "%s%s", config_mqtt_topic_prefix, sub_list[i].topic_name);
			mqtt_subscribe(&client, full_topic_name, sub_list[i].max_qos);
		}
		
		if(client.error != MQTT_OK) {
			debug("MQTT error: %s\n", mqtt_error_str(client.error));
			brssl_mqtt_close(&ctx);
			vTaskDelay(pdMS_TO_TICKS(500));
			continue;
		}
		
		while(1) {
			start_time = sdk_system_get_time();
			
			rc = mqtt_sync(&client);
			
			if(rc != MQTT_OK) {
				debug("MQTT error: %s\n", mqtt_error_str(rc));
				debug("BearSSL last error: %d\n", br_ssl_engine_last_error(&ctx.cc.eng));
				
				brssl_mqtt_close(&ctx);
				break;
			}
			
			end_time = sdk_system_get_time();
			
			cycle_count = (cycle_count + 1) % 3;
			cycle_duration[cycle_count] = SYSTIME_DIFF(start_time, end_time) / 1000U;
			mqtt_cycle_duration = ((float)(cycle_duration[0] + cycle_duration[1] + cycle_duration[2])) / 3.0;
			
			vTaskDelay(pdMS_TO_TICKS(500));
		}
		
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}
