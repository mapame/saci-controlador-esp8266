#include <espressif/esp_common.h>

#include <esp8266.h>

#include <string.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#include <httpd/httpd.h>

#include "common.h"
#include "module_manager.h"
#include "mjson.h"

#define KEY_EXPIRATION_TIME 30U * 60U

struct tcp_pcb *client_list[3] = {NULL};
int client_qty = 0;

char access_key_txt[33];
uint32_t access_key_time;

static inline uint32_t get_elapsed_time(uint32_t time) {
	uint32_t time_now = sdk_system_get_time();
	
	return (time_now < time) ? ((((uint32_t)0xFFFFFFFF) - time) + time_now + ((uint32_t)1)) : (time_now - time);
}

static void create_key() {
	uint8_t key_value[16];
	
	access_key_time = sdk_system_get_time();
	
	hwrand_fill(key_value, 16);
	
	for(int i = 0; i < 16; i++)
		sprintf(&(access_key_txt[i * 2]), "%02x", key_value[i]);
	
	access_key_txt[32] = '\0';
}

static inline void delete_key() {
	access_key_txt[0] = '\0';
}

static int check_key(const char *key_text) {
	if(get_elapsed_time(access_key_time) >= (KEY_EXPIRATION_TIME * 1000000U)) {
		delete_key();
		
		return 1;
	}
	
	if(key_text == NULL)
		return -1;
	
	if(strncmp(key_text, access_key_txt, 32))
		return 2;
	
	return 0;
}

void websocket_cb(struct tcp_pcb *pcb, uint8_t *data, u16_t data_len, uint8_t mode) {
	int client_i;
	char optxt[32];
	char aux[64];
	
	int token_type;
	int token_len;
	const char *token = NULL;

	char response[256];
	int response_len;
	
	for(client_i = 0; client_i < 3; client_i++)
		if(client_list[client_i] == pcb)
			break;
	
	if(client_i == 3)
		return;
	
	if(mjson_get_string((char*)data, data_len, "$.op", optxt, 64) <= 0)
		return;
	
	if(!strcmp(optxt, "login")) {
		if(mjson_get_string((char*)data, data_len, "$.password", aux, 64) <= 0)
			return;
		
		if(!strcmp(aux, "12345678")) {
			create_key();
			
			response_len = snprintf(response, sizeof(response), "{\"key\":\"%s\"}", access_key_txt);
		} else {
			response_len = snprintf(response, sizeof(response), "{\"error\":\"wrong_password\"}");
		}
		
		websocket_write(pcb, (uint8_t*)response, response_len, WS_TEXT_MODE);
		
		return;
	}
	
	if(mjson_get_string((char*)data, data_len, "$.key", aux, 64) != 32)
		return;
	
	if(check_key(aux)) {
		response_len = snprintf(response, sizeof(response), "{\"error\":\"invalid_key\"}");
		websocket_write(pcb, (uint8_t*)response, response_len, WS_TEXT_MODE);
		return;
	}
	
	if(!strcmp(optxt, "logout")) {
		delete_key();
		
		response_len = snprintf(response, sizeof(response), "{\"error\":\"invalid_key\"}");
		websocket_write(pcb, (uint8_t*)response, response_len, WS_TEXT_MODE);
		
	} else if(!strcmp(optxt, "nop")) {
		return;
		
	} else if(!strcmp(optxt, "restart")) {
		sdk_system_restart();
		
	} else if(!strcmp(optxt, "diagnose-write")) {
		
	}
}

void websocket_open_cb(struct tcp_pcb *pcb, const char *uri) {
	if(client_qty < 3) {
		for(int i = 0; i < 3; i++)
			if(client_list[i] == NULL) {
				client_list[i] = pcb;
				break;
			}
		
		client_qty++;
	}
}

static int create_module_json(unsigned int module_addr, char *buffer, unsigned int buffer_len) {
	char aux[20];
	unsigned int len;
	
	int channel_qty;
	char type;
	char writable;
	int port_qty;
	
	if(buffer == NULL || buffer_len < 2)
		return -1;
	
	channel_qty = module_get_info(module_addr, aux, 20);
	
	if(channel_qty <= 0)
		return -2;
	
	len = snprintf(buffer, buffer_len, "{\"address\":%u,\"name\":\"%s\",\"channels\":[", module_addr, aux);
	
	for(unsigned int channeln = 0; channeln < channel_qty; channeln++) {
		port_qty = module_get_channel_info(module_addr, channeln, aux, &type, &writable);
		
		if(port_qty <= 0)
			return -3;
		
		if(len >= buffer_len)
			return -4;
		
		len += snprintf(buffer + len, buffer_len - len, "{\"name\":\"%s\",\"type\":\"%c\",\"writable\":\"%c\",\"values\":[", aux, type, writable);
		
		for(unsigned int portn = 0; portn < port_qty; portn++) {
			if(len + 4 >= buffer_len)
				return -4;
			
			if(type == 'T') {
				strcpy(buffer + len, "\"\",");
				len += 3;
				continue;
			}
			
			if(module_get_port_value_as_text(module_addr, channeln, portn, aux, 20) < 0)
				return -5;
			
			len += snprintf(buffer + len, buffer_len - len, "%s,", aux);
		}
		
		len--; // Remove last comma
		
		if(len + 4 >= buffer_len)
			return -4;
		
		strcpy(buffer + len, "]},");
		len += 3;
	}
	
	len--; // Remove last comma
	
	if(len + 3 >= buffer_len)
		return -4;
	
	strcpy(buffer + len, "]}");
	len += 2;
	
	return len;
}

static int send_message_to_clients(const char *buffer, unsigned int buffer_len) {
	err_t result;
	
	for(int i = 0; i < 3; i++) {
		if (client_list[i] == NULL)
			continue;
		
		if(client_list[i]->state != ESTABLISHED) {
			client_list[i] = NULL;
			client_qty--;
			
			continue;
		}
		
		LOCK_TCPIP_CORE();
		result = websocket_write(client_list[i], (const unsigned char *) buffer, (uint16_t) buffer_len, WS_TEXT_MODE);
		UNLOCK_TCPIP_CORE();
		
		if(result != ERR_OK)
			return -1;
	}
	
	return 0;
}

void httpd_task(void *pvParameters) {
	int result;
	char response_buffer[1024];
	unsigned int response_len;
	
	websocket_register_callbacks((tWsOpenHandler) websocket_open_cb, (tWsHandler) websocket_cb);
	httpd_init();
	
	vTaskDelay(pdMS_TO_TICKS(100));
	
	for(;;) {
		if(sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
			vTaskDelay(pdMS_TO_TICKS(1000));
			continue;
		}
		
		response_len = snprintf(response_buffer, sizeof(response_buffer), "{\"free_heap\":%u}", (unsigned int) xPortGetFreeHeapSize());
		
		send_message_to_clients(response_buffer, response_len);
		
		for(unsigned int module_addr = 0; module_addr < 32; module_addr++) {
			response_len = sprintf(response_buffer, "{\"module_data\":");
			
			result = create_module_json(module_addr, response_buffer + response_len, sizeof(response_buffer) - response_len);
			
			if(result <= 0)
				continue;
			
			response_len += result;
			
			if(response_len + 2 >= sizeof(response_buffer))
				continue;
			
			strcpy(response_buffer + response_len, "}");
			response_len += 1;
			
			send_message_to_clients(response_buffer, response_len);
		}
		
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
