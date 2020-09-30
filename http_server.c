#include <espressif/esp_common.h>

#include <esp8266.h>

#include <string.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <message_buffer.h>

#include <httpd/httpd.h>
#include "http_client_ota.h"

#include "common.h"
#include "rtc.h"
#include "module_manager.h"
#include "mjson.h"

#define MAX_CLIENT_QTY 3

#define CLIENT_ACTION_BUFFER_SIZE 2048

#define KEY_EXPIRATION_TIME 30U * 60U

MessageBufferHandle_t client_action_buffer = NULL;

TaskHandle_t httpd_task_handle = NULL;

struct tcp_pcb *client_pcbs[MAX_CLIENT_QTY] = {NULL};
int client_is_new[MAX_CLIENT_QTY] = {0};
int client_qty = 0;
struct tcp_pcb *logged_client_pcb = NULL;

char access_key_txt[33];
uint32_t access_key_time;

extern float mm_cycle_duration;

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
	uint32_t time_now = sdk_system_get_time();
	
	if(SYSTIME_DIFF(access_key_time, time_now) >= (KEY_EXPIRATION_TIME * 1000000U)) {
		delete_key();
		
		return 1;
	}
	
	if(key_text == NULL)
		return -1;
	
	if(strncmp(key_text, access_key_txt, 32))
		return 2;
	
	return 0;
}

static int websocket_all_clients_write(int new_clients_only, const char *buffer, unsigned int buffer_len) {
	err_t result;
	
	for(int i = 0; i < MAX_CLIENT_QTY; i++) {
		if(client_pcbs[i] == NULL || TCP_STATE_IS_CLOSING(client_pcbs[i]->state))
			continue;
		
		if(client_is_new[i] != new_clients_only)
			continue;
		
		LOCK_TCPIP_CORE();
		result = websocket_write(client_pcbs[i], (const unsigned char *) buffer, (uint16_t) buffer_len, WS_TEXT_MODE);
		UNLOCK_TCPIP_CORE();
		
		if(result != ERR_OK)
			return -1;
	}
	
	return 0;
}

static int websocket_logged_client_write(const char *buffer, unsigned int buffer_len) {
	err_t result;
	
	if(logged_client_pcb == NULL || TCP_STATE_IS_CLOSING(logged_client_pcb->state))
		return -2;
		
	LOCK_TCPIP_CORE();
	result = websocket_write(logged_client_pcb, (const unsigned char *) buffer, (uint16_t) buffer_len, WS_TEXT_MODE);
	UNLOCK_TCPIP_CORE();
	
	if(result != ERR_OK)
		return -1;
	
	return 0;
}

void websocket_rcv_msg_cb(struct tcp_pcb *pcb, uint8_t *data, u16_t data_len, uint8_t mode) {
	int client_i;
	char optxt[32];
	char aux[64];
	
	char response[256];
	int response_len;
	
	for(client_i = 0; client_i < MAX_CLIENT_QTY; client_i++)
		if(client_pcbs[client_i] == pcb)
			break;
	
	if(client_i == MAX_CLIENT_QTY)
		return;
	
	if(mjson_get_string((char*)data, data_len, "$.op", optxt, 64) <= 0)
		return;
	
	if(!strcmp(optxt, "login")) {
		if(mjson_get_string((char*)data, data_len, "$.password", aux, 64) <= 0)
			return;
		
		if(!strcmp(aux, "12345678")) {
			create_key();
			
			response_len = snprintf(response, sizeof(response), "{\"key\":\"%s\"}", access_key_txt);
			
			logged_client_pcb = pcb;
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
	
	logged_client_pcb = pcb;
	
	if(!strcmp(optxt, "logout")) {
		delete_key();
		
		logged_client_pcb = NULL;
		
		response_len = snprintf(response, sizeof(response), "{\"error\":\"invalid_key\"}");
		websocket_write(pcb, (uint8_t*)response, response_len, WS_TEXT_MODE);
		
	} else if(!strcmp(optxt, "client-action")) {
		if(mjson_get_string((char*)data, data_len, "$.parameters", aux, 64) < 3)
			return;
		
		xMessageBufferSend(client_action_buffer, (void*) &aux, strlen(aux) + 1, pdMS_TO_TICKS(200));
	} else if(!strcmp(optxt, "test-key")) {
		response_len = snprintf(response, sizeof(response), "{\"server_notification\":\"key_ok\"}");
		websocket_write(pcb, (uint8_t*)response, response_len, WS_TEXT_MODE);
	}
	
}

void websocket_open_cb(struct tcp_pcb *pcb, const char *uri) {
	if(client_qty >= MAX_CLIENT_QTY)
		return; /* TODO: modify the httpd code to allow unwanted connections to be terminated */
		
	for(int i = 0; i < MAX_CLIENT_QTY; i++) {
		if(client_pcbs[i] == NULL) {
			client_pcbs[i] = pcb;
			client_is_new[i] = 1;
			client_qty++;
			break;
		}
	}
	
	xTaskNotifyGive(httpd_task_handle);
}

static int create_module_info_json(unsigned int module_addr, char *buffer, unsigned int buffer_len) {
	char aux[20];
	unsigned int len;
	
	int channel_qty;
	char type;
	char writable;
	int port_qty;
	float min, max;
	
	if(buffer == NULL || buffer_len < 2)
		return -1;
	
	channel_qty = module_get_info(module_addr, aux, 20);
	
	if(channel_qty <= 0)
		return -2;
	
	len = snprintf(buffer, buffer_len, "{\"address\":%u,\"name\":\"%s\",\"channels\":[", module_addr, aux);
	
	for(unsigned int channeln = 0; channeln < channel_qty; channeln++) {
		if(len >= buffer_len)
			return -3;
		
		port_qty = module_get_channel_info(module_addr, channeln, aux, &type, &writable);
		
		if(port_qty <= 0)
			return -4;
		
		if(module_get_channel_bounds(module_addr, channeln, &min, &max))
			return -5;
		
		len += snprintf(buffer + len, buffer_len - len, "{\"name\":\"%s\",\"type\":\"%c\",\"port_qty\":\"%d\",\"writable\":\"%c\",\"min\":%.4f,\"max\":%.4f},", aux, type, port_qty, writable, min, max);
	}
	
	if(len >= buffer_len)
		return -3;
	
	len--; // Remove last comma
	
	if(len + 3 >= buffer_len)
		return -3;
	
	strcpy(buffer + len, "]}");
	len += 2;
	
	return len;
}

static int create_module_data_json(unsigned int module_addr, char *buffer, unsigned int buffer_len) {
	char aux[20];
	unsigned int len;
	
	int channel_qty;
	char type;
	int port_qty;
	
	if(buffer == NULL || buffer_len < 2)
		return -1;
	
	channel_qty = module_get_info(module_addr, aux, 20);
	
	if(channel_qty <= 0)
		return -2;
	
	len = snprintf(buffer, buffer_len, "{\"address\":%u,\"values\":[", module_addr);
	
	for(unsigned int channeln = 0; channeln < channel_qty; channeln++) {
		if(len >= buffer_len)
			return -4;
			
		port_qty = module_get_channel_info(module_addr, channeln, aux, &type, NULL);
		
		if(port_qty <= 0)
			return -3;
		
		buffer[len++] = '[';
		
		for(unsigned int portn = 0; portn < port_qty; portn++) {
			if(len >= buffer_len)
				return -4;
			
			if(type == 'T') {
				strcpy(aux, "\"TXT\"");
			} else {
				if(module_get_port_value_as_text(module_addr, channeln, portn, aux, 20) < 0)
					return -5;
			}
			
			len += snprintf(buffer + len, buffer_len - len, "%s,", aux);
		}
		
		len--; // Remove last comma
		
		if(len + 3 >= buffer_len)
			return -4;
		
		strcpy(buffer + len, "],");
		len += 2;
	}
	
	len--; // Remove last comma
	
	if(len + 3 >= buffer_len)
		return -4;
	
	strcpy(buffer + len, "]}");
	len += 2;
	
	return len;
}

static void send_module_data(char *buffer, unsigned int buffer_len) {
	int result;
	int response_len;
	
	unsigned int module_addr = 0;
	
	while(module_addr < 32) {
		response_len = snprintf(buffer, buffer_len, "{\"module_data\":[");
		
		if(response_len >= buffer_len)
			return;
		
		while(module_addr < 32) {
			
			result = create_module_data_json(module_addr, buffer + response_len, buffer_len - response_len);
			
			if(result <= 0) {
				module_addr++;
				continue;
			}
			
			if(response_len + result + 2 > buffer_len)
				break;
			
			response_len += result;
			
			buffer[response_len++] = ',';
			
			module_addr++;
		}
		
		if(buffer[response_len] == '[')
			return;
		
		response_len--; // Remove last comma
		
		buffer[response_len++] = ']';
		buffer[response_len++] = '}';
		
		websocket_all_clients_write(0, buffer, response_len);
	}
	
	return;
}

int client_action_module_write(char *parameters) {
	char *saveptr = NULL;
	char *param_ptrs[4];
	
	int cresult = 0;
	
	unsigned int address, channel, port;
	
	char ctype, cwritable;
	
	char bvalue;
	int ivalue;
	float fvalue;
	void *vvalue;
	
	if(parameters == NULL)
		return -1;
	
	for(int i = 0; i < 4; i++) {
		param_ptrs[i] = strtok_r(parameters, ":", &saveptr);
		
		if(param_ptrs[i] == NULL)
			return -2;
		
		parameters = NULL;
	}
	
	cresult += sscanf(param_ptrs[0], "%u", &address);
	cresult += sscanf(param_ptrs[1], "%u", &channel);
	cresult += sscanf(param_ptrs[2], "%u", &port);
	
	if(cresult != 3)
		return -2;
	
	if(module_get_channel_info(address, channel, NULL, &ctype, &cwritable) <= 0)
		return -3;
	
	if(cwritable == 'N')
		return -4;
	
	switch(ctype) {
		case 'B':
			bvalue = (*param_ptrs[3] == '0') ? 0 : 1;
			vvalue = (void*) &bvalue;
			break;
		case 'I':
			if(sscanf(param_ptrs[3], "%d", &ivalue) == 1)
				vvalue = (void*) &ivalue;
			else
				return -5;
			
			break;
		case 'F':
			if(sscanf(param_ptrs[3], "%f", &fvalue) == 1)
				vvalue = (void*) &fvalue;
			else
				return -5;
			
			break;
		case 'T':
			vvalue = (void*) param_ptrs[3];
			
			break;
		default:
			return -6;
	}
	
	if(module_set_port_value(address, channel, port, vvalue))
		return -7;
	
	return 0;
}

int client_action_update_time(char *parameters) {
	uint32_t newtime;
	
	if(parameters == NULL)
		return -1;
	
	if(sscanf(parameters, "%u", &newtime) != 1)
		return -2;
	
	if(rtc_set_time((time_t) newtime))
		return -3;
	
	return 0;
}

int client_action_ota(char *parameters, char *result_buffer) {
	char *saveptr = NULL;
	ota_info http_ota_info;
	const char default_ota_port[] = "8080";
	const char default_ota_bin_filename[] = "/saci_firmware.bin";
	const char default_ota_hash_filename[] = "/saci_firmware.sha256";
	OTA_err err;
	
	if(parameters == NULL)
		return -1;
	
	http_ota_info.server = (const char*) strtok_r(parameters, ":", &saveptr);
	
	if(http_ota_info.server == NULL)
		return -2;
	
	http_ota_info.port = (const char*) strtok_r(NULL, ":", &saveptr);
	
	if(http_ota_info.port == NULL)
		http_ota_info.port = default_ota_port;
	
	http_ota_info.binary_path = default_ota_bin_filename;
	http_ota_info.sha256_path = default_ota_hash_filename;
	
	err = ota_update(&http_ota_info);
	
	if(result_buffer != NULL) {
		switch(err) {
			case OTA_DNS_LOOKUP_FALLIED:
				strcpy(result_buffer, "DNS lookup has failed.");
				break;
			case OTA_SOCKET_ALLOCATION_FALLIED:
				strcpy(result_buffer, "Impossible allocate required socket.");
				break;
			case OTA_SOCKET_CONNECTION_FALLIED:
				strcpy(result_buffer, "Server unreachable, impossible connect.");
				break;
			case OTA_SHA_DONT_MATCH:
				strcpy(result_buffer, "Checksum does not match.");
				break;
			case OTA_REQUEST_SEND_FALLIED:
				strcpy(result_buffer, "Failed to send HTTP request.");
				break;
			case OTA_DOWLOAD_SIZE_NOT_MATCH:
				strcpy(result_buffer, "Dowload size don't match server declared size.");
				break;
			case OTA_ONE_SLOT_ONLY:
				strcpy(result_buffer, "rboot has only one slot configured, OTA is not possible.");
				break;
			case OTA_FAIL_SET_NEW_SLOT:
				strcpy(result_buffer, "Failed to switch to new rom.");
				break;
			case OTA_IMAGE_VERIFY_FALLIED:
				strcpy(result_buffer, "Downloaded image is not valid.");
				break;
			case OTA_UPDATE_DONE:
				strcpy(result_buffer, "OTA process has completed, ready to restart into new ROM.");
				break;
			case OTA_HTTP_OK:
				strcpy(result_buffer, "HTTP 200, OK");
				break;
			case OTA_HTTP_NOTFOUND:
				strcpy(result_buffer, "HTTP 404, file not found.");
				break;
			default:
				sprintf(result_buffer, "Unknown error. (%d)", err);
				break;
		}
	}
	
	return (err == OTA_UPDATE_DONE) ? 0 : -3;
}

void httpd_task(void *pvParameters) {
	int result;
	
	char response_buffer[1024];
	unsigned int response_len;
	
	uint32_t start_time, end_time;
	int cycle_duration[3], cycle_count = 0;
	
	int time_to_wait;
	
	char aux[64];
	
	httpd_task_handle = xTaskGetCurrentTaskHandle();
	
	client_action_buffer = xMessageBufferCreate(CLIENT_ACTION_BUFFER_SIZE);
	
	websocket_register_callbacks((tWsOpenHandler) websocket_open_cb, (tWsHandler) websocket_rcv_msg_cb);
	httpd_init();
	
	vTaskDelay(pdMS_TO_TICKS(100));
	
	for(;;) {
		time_t rtc_time;
		float rtc_temperature;
		
		if(sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
			vTaskDelay(pdMS_TO_TICKS(500));
			continue;
		}
		
		start_time = sdk_system_get_time();
		
		if(logged_client_pcb != NULL && TCP_STATE_IS_CLOSING(logged_client_pcb->state))
			logged_client_pcb = NULL;
		
		for(int i = 0; i < MAX_CLIENT_QTY; i++)
			if(client_pcbs[i] != NULL && TCP_STATE_IS_CLOSING(client_pcbs[i]->state)) {
				client_pcbs[i] = NULL;
				client_is_new[i] = 0;
				client_qty--;
			}
		
		if(!xMessageBufferIsEmpty(client_action_buffer)) {
			xMessageBufferReceive(client_action_buffer, (void*) &aux, sizeof(aux), 0);
			
			if(!strncmp(aux, "RST", 3)) {
				response_len = snprintf(response_buffer, sizeof(response_buffer), "{\"server_notification\":\"restart\"}");
				websocket_all_clients_write(0, response_buffer, response_len);
				
				vTaskDelay(pdMS_TO_TICKS(2000));
				sdk_system_restart();
			} else if(!strncmp(aux, "MWR:", 4)) {
				client_action_module_write(aux + 4);
			} else if(!strncmp(aux, "TUP:", 4)) {
				client_action_update_time(aux + 4);
			} else if(!strncmp(aux, "SYS", 3)) {
				float avg_duration = ((float)(cycle_duration[0] + cycle_duration[1] + cycle_duration[2])) / 3.0;
				
				response_len = snprintf(response_buffer, sizeof(response_buffer), "{\"adv_system_status\":{\"fw_ver\":\"%s\",\"free_mem\":%u,\"http_cycle_duration\":%.2f,\"mm_cycle_duration\":%.2f}}", FW_VERSION, (unsigned int) xPortGetFreeHeapSize(), avg_duration, mm_cycle_duration);
				
				websocket_logged_client_write(response_buffer, response_len);
			} else if(!strncmp(aux, "OTA:", 4)) {
				char result_txt[64];
				
				client_action_ota(aux + 4, result_txt);
				
				response_len = snprintf(response_buffer, sizeof(response_buffer), "{\"page_alert\":{\"type\":\"primary\",\"message\":\"%s\"}}", result_txt);
				
				websocket_logged_client_write(response_buffer, response_len);
			}
		}
		
		if(client_qty == 0) {
			vTaskDelay(pdMS_TO_TICKS(1500));
			continue;
		}
		
		rtc_get_temp(&rtc_temperature);
		rtc_get_time(&rtc_time);
		
		response_len = snprintf(response_buffer, sizeof(response_buffer), "{\"uptime\":%u,\"temperature\":%.1f,\"time\":%u}", (xTaskGetTickCount() * portTICK_PERIOD_MS / 1000), rtc_temperature, (uint32_t) rtc_time);
		
		websocket_all_clients_write(0, response_buffer, response_len);
		
		send_module_data(response_buffer, sizeof(response_buffer));
		
		end_time = sdk_system_get_time();
		cycle_count = (cycle_count + 1) % 3;
		cycle_duration[cycle_count] = SYSTIME_DIFF(start_time, end_time) / 1000U;
		
		time_to_wait = 1000 - cycle_duration[cycle_count];
		
		if(time_to_wait < 200)
			time_to_wait = 200;
		
		if(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(time_to_wait)) != 0) {
			/* If we received a notification that means there is a new user connected */
			for(unsigned int module_addr = 0; module_addr < 32; module_addr++) {
				response_len = sprintf(response_buffer, "{\"module_info\":");
				
				result = create_module_info_json(module_addr, response_buffer + response_len, sizeof(response_buffer) - response_len);
				
				if(result <= 0)
					continue;
				
				response_len += result;
				
				if(response_len >= sizeof(response_buffer))
					continue;
				
				response_buffer[response_len++] = '}';
				
				websocket_all_clients_write(1, response_buffer, response_len);
			}
			
			
			
			for(int i = 0; i < MAX_CLIENT_QTY; i++)
				client_is_new[i] = 0;
		}
	}
}
