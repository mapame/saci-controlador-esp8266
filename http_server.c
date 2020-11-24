#include <espressif/esp_common.h>

#include <esp8266.h>

#include <string.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <message_buffer.h>
#include <queue.h>

#include <httpd/httpd.h>

#include "libs/mjson.h"

#include "common.h"
#include "rtc.h"
#include "module_manager.h"
#include "configuration.h"
#include "dashboard.h"
#include "http_ota.h"

#define MAX_CLIENT_QTY 3

#define CLIENT_ACTION_BUFFER_SIZE 2048

#define KEY_EXPIRATION_TIME 30U * 60U

extern TaskHandle_t module_manager_task_handle;
extern TaskHandle_t custom_code_task_handle;
extern TaskHandle_t httpd_task_handle;
extern TaskHandle_t mqtt_task_handle;

extern QueueHandle_t cc_command_queue;

char config_webui_password[CONFIG_STR_SIZE];

MessageBufferHandle_t client_action_buffer = NULL;

struct tcp_pcb *client_pcbs[MAX_CLIENT_QTY] = {NULL};
int client_qty = 0;
struct tcp_pcb *logged_client_pcb = NULL;
struct tcp_pcb *new_client_pcb = NULL;

char access_key_txt[33];
uint32_t access_key_time;

extern float mm_cycle_duration;
extern float cc_cycle_duration;
float http_cycle_duration;
extern float mqtt_cycle_duration;

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

static int websocket_all_clients_write(const char *buffer, unsigned int buffer_len) {
	err_t result;
	
	for(int i = 0; i < MAX_CLIENT_QTY; i++) {
		if(client_pcbs[i] == NULL || TCP_STATE_IS_CLOSING(client_pcbs[i]->state))
			continue;
		
		LOCK_TCPIP_CORE();
		result = websocket_write(client_pcbs[i], (const unsigned char *) buffer, (uint16_t) buffer_len, WS_TEXT_MODE);
		UNLOCK_TCPIP_CORE();
		
		if(result != ERR_OK)
			return -1;
	}
	
	return 0;
}

static int websocket_client_write(struct tcp_pcb *pcb, const char *buffer, unsigned int buffer_len) {
	err_t result;
	
	if(pcb == NULL || TCP_STATE_IS_CLOSING(pcb->state))
		return -2;
		
	LOCK_TCPIP_CORE();
	result = websocket_write(pcb, (const unsigned char *) buffer, (uint16_t) buffer_len, WS_TEXT_MODE);
	UNLOCK_TCPIP_CORE();
	
	if(result != ERR_OK)
		return -1;
	
	return 0;
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
		
		len += snprintf(buffer + len, buffer_len - len,	"{\"name\":\"%s\",\"type\":\"%c\",\"port_qty\":\"%d\",\"writable\":\"%c\",\"min\":%.4f,\"max\":%.4f},",
														aux, type, port_qty, writable, min, max);
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

static int create_dashboard_parameters_json(unsigned int dashboard_i, char *buffer, unsigned int buffer_len) {
	int type_i;
	unsigned int len = 0;
	
	if(buffer == NULL || buffer_len < 2)
		return -1;
	
	if(dashboard_i >= dashboard_item_qty)
		return -2;
	
	type_i = dashboard_type_get_index(dashboard_itens[dashboard_i].type_name);
	
	if(type_i < 0)
		return -3;
	
	buffer[len++] = '[';
	
	for(int p_i = 0; p_i < 4; p_i++) {
		if(dashboard_itens[dashboard_i].parameters[p_i] == NULL) {
			len += snprintf(buffer + len, buffer_len - len, "null,");
		} else {
			switch(dashboard_item_types[type_i].parameter_types[p_i]) {
				case 'I':
					len += snprintf(buffer + len, buffer_len - len, "%d,", *((int*)dashboard_itens[dashboard_i].parameters[p_i]));
					break;
				case 'F':
					len += snprintf(buffer + len, buffer_len - len, "%.4f,", *((float*)dashboard_itens[dashboard_i].parameters[p_i]));
					break;
				case 'T':
					len += snprintf(buffer + len, buffer_len - len, "\"%s\",", (char*)dashboard_itens[dashboard_i].parameters[p_i]);
					break;
				case 'N':
					break;
				default:
					break;
			}
		}
		
		if(len >= buffer_len)
			return -4;
	}
	
	if(buffer[len - 1] == ',')
		len--; // Remove last comma
	
	buffer[len++] = ']';
	
	return len;
}

static void send_config_forms(struct tcp_pcb *pcb, char *buffer, unsigned int buffer_len) {
	int result;
	
	unsigned int response_len;
	
	if(buffer == NULL || buffer_len < 20)
		return;
	
	response_len = snprintf(buffer, buffer_len, "{\"config_forms\":{\"qty\":%d,\"titles\":[", config_form_qty);
	
	if(response_len >= buffer_len)
		return;
	
	for(unsigned int i = 0; i < config_form_qty; i++) {
		
		result = snprintf(buffer + response_len, buffer_len - response_len, "\"%s\",", &(config_form_titles[i][0]));
		
		if(response_len + result + 3 - 1 >= buffer_len)
			break;
		
		response_len += result;
	}
	
	if(buffer[response_len - 1] == ',')
		response_len--; // Remove last comma
	
	buffer[response_len++] = ']';
	buffer[response_len++] = '}';
	buffer[response_len++] = '}';
	
	websocket_client_write(pcb, buffer, response_len);
	
	vTaskDelay(pdMS_TO_TICKS(500));
}

static void send_config_info(struct tcp_pcb *pcb, char *buffer, unsigned int buffer_len) {
	const config_info_t *config_info;
	unsigned int response_len;
	
	if(buffer == NULL || buffer_len < 20)
		return;
	
	for(unsigned int config_index = 0; config_index < CONFIG_TABLE_TOTAL_QTY; config_index++) {
		if(configuration_get_info(config_index, &config_info) < 0)
			continue;
		
		response_len = snprintf(buffer, buffer_len,	"{\"config_info\":{\"formn\":%d,\"name\":\"%s\",\"dname\":\"%s\",\"type\":\"%c\",""\"min\":%.5f,\"max\":%.5f,\"req_restart\":\"%c\"}}",
													config_info->formn, config_info->name, config_info->dname, config_info->type, config_info->min, config_info->max, (config_info->require_restart ? 'Y' : 'N'));
		
		if(response_len >= buffer_len)
			continue;
		
		websocket_client_write(pcb, buffer, response_len);
		
		vTaskDelay(pdMS_TO_TICKS(120));
	}
}

static void send_config_values(struct tcp_pcb *pcb, int index, char *buffer, unsigned int buffer_len) {
	const config_info_t *config_info;
	char config_value[65];
	unsigned int response_len;
	
	for(unsigned int config_index = ((index < 0) ? 0 : index); config_index < CONFIG_TABLE_TOTAL_QTY; config_index++) {
		if(configuration_get_info(config_index, &config_info) < 0)
			continue;
		
		if(configuration_read_value(config_index, config_value, sizeof(config_value)) < 0)
			continue;
		
		if(strlen(config_value) == 0)
			continue;
		
		response_len = snprintf(buffer, buffer_len,	"{\"config_data\":{\"name\":\"%s\",\"value\":\"%s\"}}",
													config_info->name, config_value);
		
		if(response_len >= buffer_len)
			continue;
		
		websocket_client_write(pcb, buffer, response_len);
		
		vTaskDelay(pdMS_TO_TICKS(80));
		
		if(index >= 0)
			break;
	}
}

static void send_module_info(struct tcp_pcb *pcb, char *buffer, unsigned int buffer_len) {
	int result;
	unsigned int response_len;
	
	for(unsigned int module_addr = 0; module_addr < MAX_MODULE_QTY; module_addr++) {
		response_len = sprintf(buffer, "{\"module_info\":");
		
		result = create_module_info_json(module_addr, buffer + response_len, buffer_len - response_len);
		
		if(result <= 0)
			continue;
		
		response_len += result;
		
		if(response_len >= buffer_len)
			continue;
		
		buffer[response_len++] = '}';
		
		websocket_client_write(pcb, buffer, response_len);
		
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

static void send_module_data(char *buffer, unsigned int buffer_len) {
	int result;
	unsigned int response_len;
	
	unsigned int module_addr = 0;
	
	if(buffer == NULL || buffer_len < 20)
		return;
	
	while(module_addr < MAX_MODULE_QTY) {
		response_len = snprintf(buffer, buffer_len, "{\"module_data\":[");
		
		while(module_addr < MAX_MODULE_QTY) {
			
			result = create_module_data_json(module_addr, buffer + response_len, buffer_len - response_len);
			
			if(result > 0) {
				
				if(response_len + result + 2 > buffer_len)
					break;
				
				response_len += result;
				
				buffer[response_len++] = ',';
			}
			
			module_addr++;
		}
		
		if(buffer[response_len - 1] == '[') // Do not send empty array
			return;
		
		if(buffer[response_len - 1] == ',')
			response_len--; // Remove last comma
		
		buffer[response_len++] = ']';
		buffer[response_len++] = '}';
		
		websocket_all_clients_write(buffer, response_len);
		
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

static void send_dashboard_lines(struct tcp_pcb *pcb, char *buffer, unsigned int buffer_len) {
	int result;
	
	unsigned int response_len;
	
	if(buffer == NULL || buffer_len < 20)
		return;
	
	response_len = snprintf(buffer, buffer_len, "{\"dashboard_lines\":{\"qty\":%d,\"titles\":[", dashboard_line_title_qty);
	
	if(response_len >= buffer_len)
		return;
	
	for(unsigned int i = 0; i < dashboard_line_title_qty; i++) {
		
		result = snprintf(buffer + response_len, buffer_len - response_len, "\"%s\",", &(dashboard_line_titles[i][0]));
		
		if(response_len + result + 3 - 1 >= buffer_len)
			break;
		
		response_len += result;
	}
	
	if(buffer[response_len - 1] == ',')
		response_len--; // Remove last comma
	
	buffer[response_len++] = ']';
	buffer[response_len++] = '}';
	buffer[response_len++] = '}';
	
	websocket_client_write(pcb, buffer, response_len);
	
	vTaskDelay(pdMS_TO_TICKS(300));
}

static void send_dashboard_info(struct tcp_pcb *pcb, char *buffer, unsigned int buffer_len) {
	int result;
	unsigned int response_len;
	
	if(buffer == NULL || buffer_len < 20)
		return;
	
	for(unsigned int di = 0; di < dashboard_item_qty; di++) {
		response_len = snprintf(buffer, buffer_len,	"{\"dashboard_info\":{\"number\":%u,\"line\":%u,\"width\":[%u,%u,%u],\"type\":\"%s\",\"dname\":\"%s\",\"parameters\":",
													di, dashboard_itens[di].line, dashboard_itens[di].width[0], dashboard_itens[di].width[1], dashboard_itens[di].width[2], dashboard_itens[di].type_name, dashboard_itens[di].dname);
		
		if(response_len >= buffer_len)
			continue;
		
		result = create_dashboard_parameters_json(di, buffer + response_len, buffer_len - response_len);
		
		if(result <= 0 || response_len + result + 2 >= buffer_len)
			continue;
		
		response_len += result;
		
		buffer[response_len++] = '}';
		buffer[response_len++] = '}';
		
		websocket_client_write(pcb, buffer, response_len);
		
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

static void send_dashboard_parameters(char *buffer, unsigned int buffer_len) {
	int result;
	unsigned int response_len;
	
	if(buffer == NULL || buffer_len < 20)
		return;
	
	for(unsigned int di = 0; di < dashboard_item_qty; di++) {
		response_len = snprintf(buffer, buffer_len,	"{\"dashboard_parameter\":{\"number\":%u,\"parameters\":", di);
		
		if(response_len >= buffer_len)
			continue;
		
		result = create_dashboard_parameters_json(di, buffer + response_len, buffer_len - response_len);
		
		if(result <= 0 || response_len + result + 2 >= buffer_len)
			continue;
		
		response_len += result;
		
		buffer[response_len++] = '}';
		buffer[response_len++] = '}';
		
		websocket_all_clients_write(buffer, response_len);
		
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}


void client_action_restart() {
	const char restart_message[] = "{\"server_notification\":\"restart\"}";
	
	websocket_client_write(logged_client_pcb, restart_message, strlen(restart_message));
	
	vTaskDelay(pdMS_TO_TICKS(2000));
	sdk_system_restart();
}

int client_action_module_write(char *parameters) {
	char *saveptr = NULL;
	char *param_ptrs[4];
	
	int cresult = 0;
	
	unsigned int address, channel, port;
	
	
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
	
	if(module_set_port_value_as_text(address, channel, port, param_ptrs[3]) < 0)
		return -7;
	
	return 0;
}

int client_action_config_write(char *parameters) {
	char *value_ptr;
	unsigned int config_index;
	
	if(parameters == NULL)
		return -1;
	
	value_ptr = strchr(parameters, ':');
	
	if(value_ptr == NULL)
		return -2;
	
	*value_ptr = '\0';
	
	value_ptr++;
	
	config_index = configuration_get_index((const char*)parameters);
	
	if(config_index < 0)
		return -3;
	
	configuration_write_value(config_index, (const char*)value_ptr);
	
	return config_index;
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

int client_action_dashboard_button(char *parameters) {
	if(parameters == NULL)
		return -1;
	
	xQueueSend(cc_command_queue, (void *)parameters, pdMS_TO_TICKS(500));
	
	return 0;
}

int client_action_ota(char *parameters, const char **message_ptr) {
	char *saveptr = NULL;
	OTA_config_t ota_config;
	OTA_err err;
	
	if(parameters == NULL)
		return -1;
	
	ota_config.server = (const char*) strtok_r(parameters, ":", &saveptr);
	
	if(ota_config.server == NULL)
		return -2;
	
	ota_config.port = (const char*) strtok_r(NULL, ":", &saveptr);
	
	if(ota_config.port == NULL)
		return -2;
	
	ota_config.path = (const char*) strtok_r(NULL, ":", &saveptr);
	
	if(ota_config.path == NULL)
		return -2;
	
	ota_config.hash_str = (const char*) strtok_r(NULL, ":", &saveptr);
	
	if(ota_config.hash_str == NULL)
		return -2;
	
	err = ota_update(&ota_config);
	
	if(message_ptr != NULL)
		*message_ptr = ota_err_str(err);
	
	return (err == OTA_UPDATE_DONE) ? 0 : -3;
}

static inline void process_client_actions(char *buffer, unsigned int buffer_len) {
	char auxbuffer[150];
	int response_len;
	
	while(!xMessageBufferIsEmpty(client_action_buffer)) {
		xMessageBufferReceive(client_action_buffer, (void*) &auxbuffer, sizeof(auxbuffer), 0);
		
		if(!strncmp(auxbuffer, "RST:", 4)) {
			client_action_restart();
			
		} else if(!strncmp(auxbuffer, "MWR:", 4)) {
			client_action_module_write(auxbuffer + 4);
			
		} else if(!strncmp(auxbuffer, "TUP:", 4)) {
			client_action_update_time(auxbuffer + 4);
			
		} else if(!strncmp(auxbuffer, "SYSI:", 5)) {
			response_len = snprintf(buffer, buffer_len,	"{\"system_info\":{\"fw_ver\":\"%s\",\"cc_ver\":\"%s\",\"uptime\":%u,\"free_heap\":%u,\"cycle_duration\":[%.2f,%.2f,%.2f,%.2f],\"task_shwm\":[%u,%u,%u,%u]}}",
																				FW_VERSION, custom_code_version,
																				(xTaskGetTickCount() / configTICK_RATE_HZ), (unsigned int) xPortGetFreeHeapSize(),
																				http_cycle_duration, mm_cycle_duration, cc_cycle_duration, mqtt_cycle_duration,
																				(unsigned int)uxTaskGetStackHighWaterMark(httpd_task_handle),
																				(unsigned int)uxTaskGetStackHighWaterMark(module_manager_task_handle),
																				(unsigned int)uxTaskGetStackHighWaterMark(custom_code_task_handle),
																				(unsigned int)uxTaskGetStackHighWaterMark(mqtt_task_handle));
			
			websocket_client_write(logged_client_pcb, buffer, response_len);
			
		} else if(!strncmp(auxbuffer, "OTA:", 4)) {
			const char *result_msg = NULL;
			int result;
			
			result = client_action_ota(auxbuffer + 4, &result_msg);
			
			if(result == 0)
				response_len = snprintf(buffer, buffer_len, "{\"server_notification\":\"ota_done\"}");
			else
				response_len = snprintf(buffer, buffer_len, "{\"server_notification\":\"ota_failed\",\"details\":\"%s\"}", ((result_msg == NULL) ? "" : result_msg));
			
			websocket_client_write(logged_client_pcb, buffer, response_len);
			
			if(result == 0)
				client_action_restart();
			
		} else if(!strncmp(auxbuffer, "CFGI:", 5)) {
			const char done_message[] = "{\"server_notification\":\"config_info_done\"}";
			
			send_config_forms(logged_client_pcb, buffer, buffer_len);
			
			send_config_info(logged_client_pcb, buffer, buffer_len);
			
			send_config_values(logged_client_pcb, -1, buffer, buffer_len);
			
			websocket_client_write(logged_client_pcb, done_message, strlen(done_message));
			
		} else if(!strncmp(auxbuffer, "CFGW:", 5)) {
			int result;
			
			result = client_action_config_write(auxbuffer + 5);
			
			if(result >= 0)
				send_config_values(logged_client_pcb, result, buffer, buffer_len);
		} else if(!strncmp(auxbuffer, "DBTN:", 5)) {
			client_action_dashboard_button(auxbuffer + 5);
		}
	}
}

void websocket_rcv_msg_cb(struct tcp_pcb *pcb, uint8_t *data, u16_t data_len, uint8_t mode) {
	int client_i;
	char optxt[32];
	char aux[150];
	
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
		
		if(!strcmp(aux, config_webui_password)) {
			create_key();
			
			response_len = snprintf(response, sizeof(response), "{\"new_key\":\"%s\"}", access_key_txt);
			
			logged_client_pcb = pcb;
		} else {
			response_len = snprintf(response, sizeof(response), "{\"server_notification\":\"wrong_password\"}");
		}
		
		websocket_write(pcb, (uint8_t*)response, response_len, WS_TEXT_MODE);
		
		return;
	}
	
	if(mjson_get_string((char*)data, data_len, "$.key", aux, 64) != 32)
		return;
	
	if(check_key(aux)) {
		response_len = snprintf(response, sizeof(response), "{\"server_notification\":\"invalid_key\"}");
		websocket_write(pcb, (uint8_t*)response, response_len, WS_TEXT_MODE);
		return;
	}
	
	logged_client_pcb = pcb;
	
	if(!strcmp(optxt, "logout")) {
		delete_key();
		
		logged_client_pcb = NULL;
		
		response_len = snprintf(response, sizeof(response), "{\"server_notification\":\"invalid_key\"}");
		websocket_write(pcb, (uint8_t*)response, response_len, WS_TEXT_MODE);
		
	} else if(!strcmp(optxt, "action")) {
		if(mjson_get_string((char*)data, data_len, "$.parameters", aux, sizeof(aux)) < 3)
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
			client_qty++;
			break;
		}
	}
	
	if(new_client_pcb == NULL)
		new_client_pcb = pcb;
}

void httpd_task(void *pvParameters) {
	char response_buffer[1024];
	unsigned int response_len;
	
	int counter = 0;
	
	uint32_t start_time, end_time;
	int cycle_duration[3], cycle_count = 0;
	int time_to_wait;
	
	client_action_buffer = xMessageBufferCreate(CLIENT_ACTION_BUFFER_SIZE);
	
	websocket_register_callbacks((tWsOpenHandler) websocket_open_cb, (tWsHandler) websocket_rcv_msg_cb);
	httpd_init();
	
	vTaskDelay(pdMS_TO_TICKS(100));
	
	for(;;) {
		if(sdk_wifi_station_get_connect_status() != STATION_GOT_IP && ap_mode == 0) {
			vTaskDelay(pdMS_TO_TICKS(500));
			continue;
		}
		
		start_time = sdk_system_get_time();
		
		if(logged_client_pcb != NULL && TCP_STATE_IS_CLOSING(logged_client_pcb->state))
			logged_client_pcb = NULL;
		
		for(int i = 0; i < MAX_CLIENT_QTY; i++)
			if(client_pcbs[i] != NULL && TCP_STATE_IS_CLOSING(client_pcbs[i]->state)) {
				client_pcbs[i] = NULL;
				client_qty--;
			}
		
		if(new_client_pcb != NULL) {
			if(config_diagnostic_mode) {
				response_len = snprintf(response_buffer, sizeof(response_buffer), "{\"server_notification\":\"diagnostic_mode\"}");
				websocket_client_write(new_client_pcb, response_buffer, response_len);
				
				send_module_info(new_client_pcb, response_buffer, sizeof(response_buffer));
				
			} else {
				send_dashboard_lines(new_client_pcb, response_buffer, sizeof(response_buffer));
				
				send_dashboard_info(new_client_pcb, response_buffer, sizeof(response_buffer));
			}
			
			response_len = snprintf(response_buffer, sizeof(response_buffer), "{\"server_notification\":\"loading_done\"}");
			websocket_client_write(new_client_pcb, response_buffer, response_len);
			
			new_client_pcb = NULL;
		}
		
		process_client_actions(response_buffer, sizeof(response_buffer));
		
		if(client_qty == 0) {
			vTaskDelay(pdMS_TO_TICKS(1000));
			continue;
		}
		
		counter = (counter + 1) % 2;
		
		if(counter == 0) {
			time_t rtc_time;
			float rtc_temperature;
			
			rtc_get_temp(&rtc_temperature);
			rtc_get_time(&rtc_time);
			
			response_len = snprintf(response_buffer, sizeof(response_buffer), "{\"temperature\":%.1f,\"time\":%u}", rtc_temperature, (uint32_t) rtc_time);
			
			websocket_all_clients_write(response_buffer, response_len);
			
			if(config_diagnostic_mode) {
				send_module_data(response_buffer, sizeof(response_buffer));
			} else {
				send_dashboard_parameters(response_buffer, sizeof(response_buffer));
			}
		}
		
		end_time = sdk_system_get_time();
		cycle_count = (cycle_count + 1) % 3;
		cycle_duration[cycle_count] = SYSTIME_DIFF(start_time, end_time) / 1000U;
		http_cycle_duration = ((float)(cycle_duration[0] + cycle_duration[1] + cycle_duration[2])) / 3.0;
		
		time_to_wait = 1000 - cycle_duration[cycle_count];
		
		vTaskDelay(pdMS_TO_TICKS(MAX(time_to_wait, 200)));
	}
}
