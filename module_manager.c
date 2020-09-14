#include <espressif/esp_common.h>
#include <espressif/esp_misc.h>
#include <esp8266.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "common.h"
#include "comm.h"
#include "module_manager.h"

typedef struct {
	char name[16];
	unsigned int port_qty;
	char type;
	char writable;
	float min;
	float max;
	void *values;
	char *value_update_type;
} channel_desc_t;

typedef struct {
	char name[16];
	unsigned int channel_qty;
	unsigned int uptime;
	channel_desc_t *channels;
} module_desc_t;

static int fetch_channel_info(unsigned int module_addr, channel_desc_t *channel_ptr, unsigned int qty);
static void free_channel_list(channel_desc_t *channel_ptr, unsigned int qty);
static int parse_content(char *buffer, char *value_ptrs[], unsigned int max);

SemaphoreHandle_t module_manager_mutex = NULL;

module_desc_t module_list[32];
unsigned int value_update_period_ms = 1000;
float mm_cycle_duration;
int diagnostic_mode = 1;

static int update_values() {
	int module_addr, chn, portn;
	char aux_buffer[50];
	comm_error_t error;
	int skip_channel;
	int binary_failed;
	
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -1;
	
	/* TODO: retry when communication fail */
	/* TODO: notify when a communication error happens */
	
	for(module_addr = 0; module_addr < 32; module_addr++) {
		for(chn = 0; chn < module_list[module_addr].channel_qty; chn++) {
			if(module_list[module_addr].channels[chn].type == 'T') /* Do not support reading text from modules */
				continue;
			
			if(!diagnostic_mode) {
				skip_channel = 1;
				
				for(portn = 0; portn < module_list[module_addr].channels[chn].port_qty; portn++) {
					if(module_list[module_addr].channels[chn].value_update_type[portn] != 'N') {
						skip_channel = 0;
						
						break;
					}
				}
				
				if(skip_channel)
					continue;
			}
			
			binary_failed = 0;
			
			if(module_list[module_addr].channels[chn].type == 'B') {
				sprintf(aux_buffer, "%u", chn);
				comm_send_command('B', module_addr, aux_buffer);
				error = comm_receive_response('B', aux_buffer, 50);
				
				if(error != COMM_OK || aux_buffer[0] == '\x15')
					binary_failed = 1;
			}
			
			for(portn = 0; portn < module_list[module_addr].channels[chn].port_qty; portn++) {
				if(!diagnostic_mode && module_list[module_addr].channels[chn].value_update_type[portn] == 'N')
					continue;
				
				if(module_list[module_addr].channels[chn].type != 'B' || binary_failed) {
					sprintf(aux_buffer, "%u:%u", chn, portn);
					comm_send_command('R', module_addr, aux_buffer);
					error = comm_receive_response('R', aux_buffer, 50);
					
					if(error != COMM_OK)
						continue;
					
					if(aux_buffer[0] == '\x15')
						continue;
					
				}
				
				if(module_list[module_addr].channels[chn].type == 'B') {
					((char*) module_list[module_addr].channels[chn].values)[portn] = (aux_buffer[binary_failed ? 0 : portn] == '0') ? 0 : 1;
				} else if(module_list[module_addr].channels[chn].type == 'I') {
					sscanf(aux_buffer, "%d", &(((int*) module_list[module_addr].channels[chn].values)[portn]));
				} else if(module_list[module_addr].channels[chn].type == 'F') {
					sscanf(aux_buffer, "%f", &(((float*) module_list[module_addr].channels[chn].values)[portn]));
				}
			}
		}
	}
	
	xSemaphoreGive(module_manager_mutex);
	
	return 0;
}

int module_get_info(unsigned int module_addr, char *name_buffer, unsigned int buffer_len) {
	int channel_qty;
	
	if(module_addr >= 32)
		return -1;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	channel_qty = module_list[module_addr].channel_qty;
	
	if(channel_qty > 0 && name_buffer != NULL) {
		strncpy(name_buffer, module_list[module_addr].name, buffer_len);
		name_buffer[buffer_len - 1] = '\0';
	}
	
	xSemaphoreGive(module_manager_mutex);
	
	return channel_qty;
}

int module_get_channel_info(unsigned int module_addr, unsigned int channeln, char *name_buffer, char *type, char *writable) {
	int port_qty;
	
	if(module_addr >= 32)
		return -1;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(channeln >= module_list[module_addr].channel_qty || module_list[module_addr].channels == NULL) {
		xSemaphoreGive(module_manager_mutex);
		return -3;
	}
	
	port_qty = module_list[module_addr].channels[channeln].port_qty;
	
	if(name_buffer != NULL)
		strcpy(name_buffer, module_list[module_addr].channels[channeln].name);
	
	if(type != NULL)
		*type = module_list[module_addr].channels[channeln].type;
	
	if(writable != NULL)
		*writable = module_list[module_addr].channels[channeln].writable;
	
	xSemaphoreGive(module_manager_mutex);
	
	return port_qty;
}

int module_set_port_enable_update(unsigned int module_addr, unsigned int channeln, unsigned int portn) {
	if(module_addr >= 32)
		return -1;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(channeln >= module_list[module_addr].channel_qty || module_list[module_addr].channels == NULL) {
		xSemaphoreGive(module_manager_mutex);
		return -3;
	}
	
	if(portn >= module_list[module_addr].channels[channeln].port_qty) {
		xSemaphoreGive(module_manager_mutex);
		return -4;
	}
	
	if(module_list[module_addr].channels[channeln].value_update_type == NULL) {
		xSemaphoreGive(module_manager_mutex);
		return -5;
	}
	
	module_list[module_addr].channels[channeln].value_update_type[portn] = 1;
	
	xSemaphoreGive(module_manager_mutex);
	
	return 0;
}

int module_get_channel_bounds(unsigned int module_addr, unsigned int channeln, float *min, float *max) {
	if(module_addr >= 32)
		return -1;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(channeln >= module_list[module_addr].channel_qty || module_list[module_addr].channels == NULL) {
		xSemaphoreGive(module_manager_mutex);
		return -3;
	}
	
	if(min)
		*min = module_list[module_addr].channels[channeln].min;
	
	if(max)
		*max = module_list[module_addr].channels[channeln].max;
	
	xSemaphoreGive(module_manager_mutex);
	
	return 0;
}

int module_set_port_value(unsigned int module_addr, unsigned int channeln, unsigned int portn, const void *value) {
	char aux_buffer[50];
	comm_error_t error;
	
	float maxv, minv;
	
	if(value == NULL)
		return -1;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(module_list[module_addr].channel_qty == 0 || channeln >= module_list[module_addr].channel_qty) {
		xSemaphoreGive(module_manager_mutex);
		return -3;
	}
	
	if(module_list[module_addr].channels[channeln].writable == 'N') {
		xSemaphoreGive(module_manager_mutex);
		return -4;
	}
	
	maxv = module_list[module_addr].channels[channeln].max;
	minv = module_list[module_addr].channels[channeln].min;
	
	switch(module_list[module_addr].channels[channeln].type) {
		case 'B':
			sprintf(aux_buffer, "%u:%u:%c", channeln, portn, (*((char*) value) == 0) ? '0' : '1');
			break;
		case 'I':
			sprintf(aux_buffer, "%u:%u:%d", channeln, portn, MIN(MAX(*((int*) value), (int) minv), (int) maxv));
			break;
		case 'F':
			sprintf(aux_buffer, "%u:%u:%.6f", channeln, portn, MIN(MAX(*((float*) value), minv), maxv));
			break;
		case 'T':
			snprintf(aux_buffer, 50, "%u:%u:%.*s", channeln, portn, (int) maxv, (char*) value);
			break;
		default:
			xSemaphoreGive(module_manager_mutex);
			return -5;
	}
	
	comm_send_command('W', module_addr, aux_buffer);
	error = comm_receive_response('W', aux_buffer, 50);
	
	xSemaphoreGive(module_manager_mutex);
	
	if(error != COMM_OK)
		return -6;
	
	if(aux_buffer[0] == '\x15')
		return -7;
	
	return 0;
}

int module_get_port_value_as_text(unsigned int module_addr, unsigned int channeln, unsigned int portn, char *buffer, unsigned int buffer_len) {
	if(buffer == NULL || buffer_len < 2)
		return -1;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(module_list[module_addr].channel_qty == 0 || channeln >= module_list[module_addr].channel_qty) {
		xSemaphoreGive(module_manager_mutex);
		return -3;
	}
	
	switch(module_list[module_addr].channels[channeln].type) {
		case 'B':
			buffer[0] = ((((char*) module_list[module_addr].channels[channeln].values)[portn] == 0) ? '0' : '1');
			buffer[1] = '\0';
			break;
		case 'I':
			snprintf(buffer, buffer_len, "%d", ((int*) module_list[module_addr].channels[channeln].values)[portn]);
			break;
		case 'F':
			snprintf(buffer, buffer_len, "%.5f", ((float*) module_list[module_addr].channels[channeln].values)[portn]);
			break;
		case 'T': /* Do not support reading text from modules */
			buffer[0] = '\0';
			break;
		default:
			xSemaphoreGive(module_manager_mutex);
			return -4;
	}
	
	xSemaphoreGive(module_manager_mutex);
	
	return 0;
}

int module_get_port_value(unsigned int module_addr, unsigned int channeln, unsigned int portn, void *value_buffer) {
	if(value_buffer == NULL)
		return -1;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(module_list[module_addr].channel_qty == 0 || channeln >= module_list[module_addr].channel_qty) {
		xSemaphoreGive(module_manager_mutex);
		return -3;
	}
	
	switch(module_list[module_addr].channels[channeln].type) {
		case 'B':
			*((char*) value_buffer) = ((char*) module_list[module_addr].channels[channeln].values)[portn];
			break;
		case 'I':
			*((int*) value_buffer) = ((int*) module_list[module_addr].channels[channeln].values)[portn];
			break;
		case 'F':
			*((float*) value_buffer) = ((float*) module_list[module_addr].channels[channeln].values)[portn];
			break;
		case 'T': /* Do not support reading text from modules */
			break;
		default:
			xSemaphoreGive(module_manager_mutex);
			return -4;
	}
	
	xSemaphoreGive(module_manager_mutex);
	
	return 0;
}

static int parse_content(char *buffer, char *value_ptrs[], unsigned int max) {
	if(max == 0)
		return 0;
	
	value_ptrs[0] = buffer;
	
	for(int i = 1; i < max; i++) {
		value_ptrs[i] = strchr(value_ptrs[i - 1], ':');
		
		if(value_ptrs[i] == NULL)
			return i;
		
		*(value_ptrs[i]) = '\0';
		
		value_ptrs[i]++;
	}
	
	return max;
}

static void free_channel_list(channel_desc_t *channel_ptr, unsigned int qty) {
	if(channel_ptr == NULL)
		return;
	
	for(int ch = 0; ch < qty; ch++) {
		free(channel_ptr[ch].value_update_type);
		free(channel_ptr[ch].values);
	}
	
	free(channel_ptr);
}

static int create_module_list() {
	unsigned int mod_addr;
	comm_error_t error;
	
	char aux_buffer[50];
	char *aux_ptrs[6];
	int aux_result;
	
	int mod_qty = 0;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -1;
	
	for(mod_addr = 0; mod_addr < 32; mod_addr++) {
		free_channel_list(module_list[mod_addr].channels, module_list[mod_addr].channel_qty);
		module_list[mod_addr].channel_qty = 0;
	}
	
	/* TODO: retry when communication fail */
	
	for(mod_addr = 0; mod_addr < 32; mod_addr++) {
		comm_send_command('P', mod_addr, "");
		
		error = comm_receive_response('P', aux_buffer, 50);
		
		if(error != COMM_OK) {
			if(error != COMM_ERROR_TIMEOUT)
				debug("Module address %u -> Unexpected comm error (%s)", mod_addr, comm_error_str(error));
			
			continue;
		}
		
		if(aux_buffer[0] == '\x15')
			continue;
		
		if(parse_content(aux_buffer, aux_ptrs, 3) != 3)
			continue;
		
		if(strlen(aux_ptrs[0]) < 1 || strlen(aux_ptrs[1]) < 1 || strlen(aux_ptrs[2]) < 1)
			continue;
		
		strncpy(module_list[mod_addr].name, aux_ptrs[0], 15);
		module_list[mod_addr].name[15] = '\0';
		
		if(sscanf(aux_ptrs[1], "%u", &(module_list[mod_addr].channel_qty)) != 1)
			continue;
		
		if(sscanf(aux_ptrs[2], "%u", &(module_list[mod_addr].uptime)) != 1)
			continue;
		
		module_list[mod_addr].channels = (channel_desc_t*) malloc(sizeof(channel_desc_t) * module_list[mod_addr].channel_qty);
		
		if(module_list[mod_addr].channels == NULL) {
			debug("Out of memory!\n");
			break;
		}
		
		aux_result = fetch_channel_info(mod_addr, module_list[mod_addr].channels, module_list[mod_addr].channel_qty);
		
		if(aux_result == module_list[mod_addr].channel_qty) {
			mod_qty++;
		} else {
			free_channel_list(module_list[mod_addr].channels, aux_result);
			module_list[mod_addr].channel_qty = 0;
		}
	}
	
	xSemaphoreGive(module_manager_mutex);
	
	return mod_qty;
}

static int fetch_channel_info(unsigned int module_addr, channel_desc_t *channel_ptr, unsigned int qty) {
	comm_error_t error;
	
	char aux_buffer[50];
	char *aux_ptrs[6];
	
	int ch;
	
	size_t value_size;
	
	for(ch = 0; ch < qty; ch++) {
		channel_ptr[ch].value_update_type = NULL;
		channel_ptr[ch].values = NULL;
	}
	
	for(ch = 0; ch < qty; ch++) {
		sprintf(aux_buffer, "%u", ch);
		comm_send_command('D', module_addr, aux_buffer);
		
		error = comm_receive_response('D', aux_buffer, 50);
		
		if(error != COMM_OK) {
			debug("Error receiving response! (%s)", comm_error_str(error));
			break;
		}
		
		if(parse_content(aux_buffer, aux_ptrs, 6) != 6)
			break;
		
		if(strlen(aux_ptrs[0]) < 1)
			break;
		
		strncpy(channel_ptr[ch].name, aux_ptrs[0], 15);
		channel_ptr[ch].name[15] = '\0';
		
		if(sscanf(aux_ptrs[1], "%u", &(channel_ptr[ch].port_qty)) != 1)
			break;
		
		if(channel_ptr[ch].port_qty == 0)
			break;
		
		channel_ptr[ch].type = *(aux_ptrs[2]);
		channel_ptr[ch].writable = *(aux_ptrs[3]);
		
		if(channel_ptr[ch].writable != 'Y' && channel_ptr[ch].writable != 'N')
			break;
		
		if(sscanf(aux_ptrs[4], "%f", &(channel_ptr[ch].min)) != 1)
			break;
		
		if(sscanf(aux_ptrs[5], "%f", &(channel_ptr[ch].max)) != 1)
			break;
		
		if(channel_ptr[ch].type != 'T') { /* Do not support reading text from modules */
			channel_ptr[ch].value_update_type = (char*) malloc(sizeof(char) * channel_ptr[ch].port_qty);
			
			if(channel_ptr[ch].value_update_type == NULL) {
				debug("Out of memory!\n");
				break;
			}
			
			memset(channel_ptr[ch].value_update_type, 'N', sizeof(char) * channel_ptr[ch].port_qty);
		}
		
		if(channel_ptr[ch].type == 'B') {
			value_size = sizeof(char);
		} else if(channel_ptr[ch].type == 'I') {
			value_size = sizeof(int);
		} else if(channel_ptr[ch].type == 'F') {
			value_size = sizeof(float);
		} else { /* Do not support reading text from modules */
			value_size = 0;
		}
		
		channel_ptr[ch].values = malloc(value_size * channel_ptr[ch].port_qty);
		
		if(value_size != 0 && channel_ptr[ch].values == NULL) {
			debug("Out of memory!\n");
			break;
		}
	}
	
	return ch;
}

void module_manager_task(void *pvParameters) {
	uint32_t start_time, end_time;
	int cycle_duration[3], cycle_count = 0;
	
	module_manager_mutex = xSemaphoreCreateMutex();
	
	for(int mod_addr = 0; mod_addr < 32; mod_addr++)
		module_list[mod_addr].channel_qty = 0;
	
	create_module_list();
	
	while(1) {
		start_time = sdk_system_get_time();
		
		update_values();
		
		end_time = sdk_system_get_time();
		
		cycle_count = (cycle_count + 1) % 3;
		cycle_duration[cycle_count] = SYSTIME_DIFF(start_time, end_time) / 1000U;
		mm_cycle_duration = ((float)(cycle_duration[0] + cycle_duration[1] + cycle_duration[2])) / 3.0;
		
		if(cycle_duration[cycle_count] < value_update_period_ms)
			vTaskDelay(pdMS_TO_TICKS(value_update_period_ms - cycle_duration[cycle_count]));
		else
			vTaskDelay(pdMS_TO_TICKS(100));
		
	}
}
