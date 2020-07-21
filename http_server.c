#include <espressif/esp_common.h>

#include <esp8266.h>

#include <string.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#include <httpd/httpd.h>

#include "common.h"
#include "module_manager.h"

struct tcp_pcb *client_list[3] = {NULL};
int client_qty = 0;

void websocket_cb(struct tcp_pcb *pcb, uint8_t *data, u16_t data_len, uint8_t mode) {
	char response[3] = "ok";
	
	for(int i = 0; i < 3; i++)
		if(client_list[i] == pcb) {
			websocket_write(pcb, (uint8_t*)response, 2, WS_TEXT_MODE);
			break;
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
	char access;
	int port_qty;
	
	if(buffer == NULL || buffer_len < 2)
		return -1;
	
	channel_qty = module_get_info(module_addr, aux, 20);
	
	if(channel_qty <= 0)
		return -2;
	
	len = snprintf(buffer, buffer_len, "{\"address\":%u,\"name\":\"%s\",\"channels\":[", module_addr, aux);
	
	for(unsigned int channeln = 0; channeln < channel_qty; channeln++) {
		port_qty = module_get_channel_info(module_addr, channeln, aux, &type, &access);
		
		if(port_qty <= 0)
			return -3;
		
		if(len >= buffer_len)
			return -4;
		
		len += snprintf(buffer + len, buffer_len - len, "{\"name\":\"%s\",\"type\":\"%c\",\"access\":\"%c\",\"values\":[", aux, type, access);
		
		for(unsigned int portn = 0; portn < port_qty; portn++) {
			if(len + 4 >= buffer_len)
				return -4;
			
			if(type == 'T') {
				strcpy(buffer + len, "\"\",");
				len += 3;
				continue;
			}
			
			if(get_port_value_text(module_addr, channeln, portn, aux, 20) < 0)
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
