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

#define MQTT_SEND_BUFFER_SIZE 1024
#define MQTT_RECV_BUFFER_SIZE 1024

#define MQTT_HOSTNAME "io.adafruit.com"
#define MQTT_PORT "8883"

#define MQTT_STATUS_TOPIC "status"
#define MQTT_STATUS_ONLINE_MSG "Online"
#define MQTT_STATUS_OFFLINE_MSG "Offline"


extern QueueHandle_t cc_command_queue;

char config_mqtt_username[CONFIG_STR_SIZE];
char config_mqtt_password[CONFIG_STR_SIZE];
char config_mqtt_clientid[CONFIG_STR_SIZE];
char config_mqtt_topic_prefix[CONFIG_STR_SIZE];

bearssl_context *ctx = NULL;
struct mqtt_client *client = NULL;

uint8_t *mqtt_sendbuf = NULL;
uint8_t *mqtt_recvbuf = NULL;

float mqtt_cycle_duration;


int mqtt_task_publish_text(const char* topic, const char* text, int qos, int retain) {
	char full_topic_name[128];
	uint8_t publish_flags = 0;
	
	if(topic == NULL || text == NULL)
		return -1;
	
	if(client == NULL || client->error != MQTT_OK)
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
	
	mqtt_publish(client, full_topic_name, text, strlen(text), publish_flags);
	
	/*
	if(client->error != MQTT_OK) {
		printf("MQTT Error: %s\n", mqtt_error_str(client->error));
	}
	*/
	
	return (client->error == MQTT_OK) ? 0 : -3;
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
	int prefix_len = strlen(config_mqtt_topic_prefix);
	char message_cpy[64];
	
	if(received->topic_name_size <= prefix_len || received->application_message_size >= sizeof(message_cpy))
		return;
	
	memcpy(message_cpy, received->application_message, received->application_message_size);
	message_cpy[received->application_message_size] = '\0';
	
	if(!strncmp(((char*)received->topic_name) + prefix_len, "comandos", received->topic_name_size - prefix_len)) {
		xQueueSend(cc_command_queue, message_cpy, 0);
	}
}

void mqtt_task(void *pvParameters) {
	uint8_t mqtt_connect_flags = MQTT_CONNECT_CLEAN_SESSION | MQTT_CONNECT_WILL_QOS_0 | MQTT_CONNECT_WILL_RETAIN;
	time_t rtc_time;
	char full_topic_name[128];
	int rc;
	
	uint32_t start_time, end_time;
	int cycle_duration[3], cycle_count = 0;
	
	while(ctx == NULL || client == NULL || mqtt_sendbuf == NULL || mqtt_recvbuf == NULL) {
		ctx = (bearssl_context*) realloc(ctx, sizeof(bearssl_context));
		client = (struct mqtt_client*) realloc(client, sizeof(struct mqtt_client));
		
		mqtt_sendbuf = (uint8_t*) realloc(mqtt_sendbuf, sizeof(uint8_t) * MQTT_SEND_BUFFER_SIZE);
		mqtt_recvbuf = (uint8_t*) realloc(mqtt_recvbuf, sizeof(uint8_t) * MQTT_RECV_BUFFER_SIZE);
		
		vTaskDelay(pdMS_TO_TICKS(500));
	}
	
	brssl_mqtt_init(ctx, 512, TAs, TAs_NUM);
	
	while(1) {
		if(sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
			vTaskDelay(pdMS_TO_TICKS(500));
			continue;
		}
		
		rtc_get_time(&rtc_time);
		
		if(brssl_mqtt_connect(ctx, MQTT_HOSTNAME, MQTT_PORT, rtc_time)!= 0) {
			vTaskDelay(pdMS_TO_TICKS(500));
			continue;
		}
		
		snprintf(full_topic_name, sizeof(full_topic_name), "%s%s", config_mqtt_topic_prefix, MQTT_STATUS_TOPIC);
		
		mqtt_init(client, ctx, mqtt_sendbuf, MQTT_SEND_BUFFER_SIZE, mqtt_recvbuf, MQTT_RECV_BUFFER_SIZE, sub_callback);
		mqtt_connect(client, config_mqtt_clientid, full_topic_name, (const void*) &(MQTT_STATUS_OFFLINE_MSG), strlen(MQTT_STATUS_OFFLINE_MSG), config_mqtt_username, config_mqtt_password, mqtt_connect_flags, 300);
		
		snprintf(full_topic_name, sizeof(full_topic_name), "%s%s", config_mqtt_topic_prefix, "comandos");
		mqtt_subscribe(client, full_topic_name, 0);
		
		if(client->error != MQTT_OK) {
			debug("MQTT error: %s\n", mqtt_error_str(client->error));
			brssl_mqtt_close(ctx);
			vTaskDelay(pdMS_TO_TICKS(500));
			continue;
		}
		
		mqtt_task_publish_text("status", MQTT_STATUS_ONLINE_MSG, 1, 0);
		
		while(1) {
			start_time = sdk_system_get_time();
			
			rc = mqtt_sync(client);
			
			if(rc != MQTT_OK) {
				debug("MQTT error: %s\n", mqtt_error_str(rc));
				debug("BearSSL last error: %d\n", br_ssl_engine_last_error(&(ctx->cc.eng)));
				
				brssl_mqtt_close(ctx);
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
