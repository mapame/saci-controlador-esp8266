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
static int set_port_value(unsigned int module_addr, unsigned int channeln, unsigned int portn, const void *value);

SemaphoreHandle_t module_manager_mutex = NULL;

extern TaskHandle_t custom_code_task_handle;

module_desc_t module_list[MAX_MODULE_QTY];
unsigned int value_update_period_ms = 1000;
float mm_cycle_duration;

static int update_values(unsigned int force_read) {
	int module_addr, chn, portn;
	char aux_buffer[50];
	comm_error_t error;
	int skip_channel;
	int binary_failed;
	
	char aux_b;
	int  aux_i;
	float aux_f;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -1;
	
	/* TODO: retry when communication fail */
	/* TODO: notify when a communication error happens */
	
	for(module_addr = 0; module_addr < MAX_MODULE_QTY; module_addr++) {
		for(chn = 0; chn < module_list[module_addr].channel_qty; chn++) {
			if(module_list[module_addr].channels[chn].type == 'T') /* Do not support reading text from modules */
				continue;
			
			if(!force_read) {
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
			
			/* If the data is binary, we can try to read multiple ports in a single operation */
			binary_failed = 0;
			if(module_list[module_addr].channels[chn].type == 'B' && module_list[module_addr].channels[chn].port_qty < 50) {
				sprintf(aux_buffer, "%u", chn);
				comm_send_command('B', module_addr, aux_buffer);
				error = comm_receive_response('B', aux_buffer, 50);
				
				if(error != COMM_OK || aux_buffer[0] == '\x15')
					binary_failed = 1; /* If the binary read failed we must read using the regular read operation */
			}
			
			for(portn = 0; portn < module_list[module_addr].channels[chn].port_qty; portn++) {
				if(!force_read && module_list[module_addr].channels[chn].value_update_type[portn] == 'N')
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
					aux_b = (aux_buffer[binary_failed ? 0 : portn] == '0') ? 0 : 1;
					
					if(module_list[module_addr].channels[chn].value_update_type[portn] == 'R' || force_read)
						((char*) module_list[module_addr].channels[chn].values)[portn] = aux_b;
					else if(aux_b != ((char*) module_list[module_addr].channels[chn].values)[portn])
						set_port_value(module_addr, chn, portn, (module_list[module_addr].channels[chn].values + portn * sizeof(char)));
					
				} else if(module_list[module_addr].channels[chn].type == 'I') {
					if(sscanf(aux_buffer, "%d", &aux_i) != 1)
						continue;
					
					if(module_list[module_addr].channels[chn].value_update_type[portn] == 'R' || force_read)
						((int*) module_list[module_addr].channels[chn].values)[portn] = aux_i;
					else if(aux_i != ((int*) module_list[module_addr].channels[chn].values)[portn])
						set_port_value(module_addr, chn, portn, (module_list[module_addr].channels[chn].values + portn * sizeof(int)));
					
				} else if(module_list[module_addr].channels[chn].type == 'F') {
					if(sscanf(aux_buffer, "%f", &aux_f) != 1)
						continue;
					
					if(module_list[module_addr].channels[chn].value_update_type[portn] == 'R' || force_read)
						((float*) module_list[module_addr].channels[chn].values)[portn] = aux_f;
					else if(aux_f != ((float*) module_list[module_addr].channels[chn].values)[portn])
						set_port_value(module_addr, chn, portn, (module_list[module_addr].channels[chn].values + portn * sizeof(float)));
				}
			}
		}
	}
	
	xSemaphoreGive(module_manager_mutex);
	
	return 0;
}

int module_get_info(unsigned int module_addr, char *name_buffer, unsigned int buffer_len) {
	int channel_qty;
	
	if(module_addr >= MAX_MODULE_QTY)
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
	
	if(module_addr >= MAX_MODULE_QTY)
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

int module_set_port_update_type(unsigned int module_addr, unsigned int channeln, unsigned int portn, char update_type) {
	if(module_addr >= MAX_MODULE_QTY)
		return -1;
	
	if(update_type != 'N' && update_type != 'W' && update_type != 'R')
		return -5;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(channeln >= module_list[module_addr].channel_qty || portn >= module_list[module_addr].channels[channeln].port_qty) {
		xSemaphoreGive(module_manager_mutex);
		return -3;
	}
	
	if(portn >= module_list[module_addr].channels[channeln].port_qty) {
		xSemaphoreGive(module_manager_mutex);
		return -4;
	}
	
	if(update_type == 'W' && module_list[module_addr].channels[channeln].writable == 'N') {
		xSemaphoreGive(module_manager_mutex);
		return -6;
	}
	
	if(module_list[module_addr].channels[channeln].value_update_type == NULL) {
		xSemaphoreGive(module_manager_mutex);
		return -7;
	}
	
	module_list[module_addr].channels[channeln].value_update_type[portn] = update_type;
	
	xSemaphoreGive(module_manager_mutex);
	
	return 0;
}

int module_get_channel_bounds(unsigned int module_addr, unsigned int channeln, float *min, float *max) {
	if(module_addr >= MAX_MODULE_QTY)
		return -1;
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(channeln >= module_list[module_addr].channel_qty) {
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

static int set_port_value(unsigned int module_addr, unsigned int channeln, unsigned int portn, const void *value) {
	float maxv, minv;
	char aux_buffer[50];
	comm_error_t error;
	
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
			break;
	}
	
	comm_send_command('W', module_addr, aux_buffer);
	error = comm_receive_response('W', aux_buffer, 50);
	
	if(error != COMM_OK)
		return -1;
	
	if(aux_buffer[0] == '\x15')
		return -2;
	
	switch(module_list[module_addr].channels[channeln].type) {
		case 'B':
			((char*) module_list[module_addr].channels[channeln].values)[portn] = *((char*) value);
			break;
		case 'I':
			((int*) module_list[module_addr].channels[channeln].values)[portn] = *((int*) value);
			break;
		case 'F':
			((float*) module_list[module_addr].channels[channeln].values)[portn] = *((float*) value);
			break;
		case 'T': /* Do not support reading text from modules */
			break;
		default:
			break;
	}
	
	return 0;
}

int module_set_port_value(unsigned int module_addr, unsigned int channeln, unsigned int portn, const void *value) {
	if(value == NULL)
		return -1;
	
	if(module_addr >= MAX_MODULE_QTY) {
		return -3;
	}
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(channeln >= module_list[module_addr].channel_qty || portn >= module_list[module_addr].channels[channeln].port_qty) {
		xSemaphoreGive(module_manager_mutex);
		return -3;
	}
	
	if(module_list[module_addr].channels[channeln].writable == 'N') {
		xSemaphoreGive(module_manager_mutex);
		return -4;
	}
	
	if(set_port_value(module_addr, channeln, portn, value) < 0) {
		xSemaphoreGive(module_manager_mutex);
		return -5;
	}
	
	xSemaphoreGive(module_manager_mutex);
	
	return 0;
}

int module_set_port_value_as_text(unsigned int module_addr, unsigned int channeln, unsigned int portn, const char *buffer) {
	char bvalue;
	int ivalue;
	float fvalue;
	void *value_ptr = NULL;
	
	if(buffer == NULL || buffer[0] == '\0')
		return -1;
	
	if(module_addr >= MAX_MODULE_QTY) {
		return -3;
	}
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(channeln >= module_list[module_addr].channel_qty || portn >= module_list[module_addr].channels[channeln].port_qty) {
		xSemaphoreGive(module_manager_mutex);
		return -3;
	}
	
	if(module_list[module_addr].channels[channeln].writable == 'N') {
		xSemaphoreGive(module_manager_mutex);
		return -4;
	}
	
	switch(module_list[module_addr].channels[channeln].type) {
		case 'B':
			if(buffer[0] == '0' || buffer[0] == '1') {
				bvalue = (buffer[0] == '0') ? 0 : 1;
				value_ptr = (void*) &bvalue;
			}
			break;
		case 'I':
			if(sscanf(buffer, "%d", &ivalue) == 1)
				value_ptr = (void*) &ivalue;
			
			break;
		case 'F':
			if(sscanf(buffer, "%f", &fvalue) == 1)
				value_ptr = (void*) &fvalue;
			
			break;
		case 'T':
			value_ptr = (void*) buffer;
			
			break;
		default:
			return -5;
	}
	
	if(value_ptr == NULL) {
		xSemaphoreGive(module_manager_mutex);
		return -6;
	}
	
	if(set_port_value(module_addr, channeln, portn, value_ptr) < 0) {
		xSemaphoreGive(module_manager_mutex);
		return -7;
	}
	
	xSemaphoreGive(module_manager_mutex);
	
	return 0;
}

int module_get_port_value_as_text(unsigned int module_addr, unsigned int channeln, unsigned int portn, char *buffer, unsigned int buffer_len) {
	if(buffer == NULL || buffer_len < 2)
		return -1;
	
	if(module_addr >= MAX_MODULE_QTY) {
		return -3;
	}
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(channeln >= module_list[module_addr].channel_qty || portn >= module_list[module_addr].channels[channeln].port_qty) {
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
	
	if(module_addr >= MAX_MODULE_QTY) {
		return -3;
	}
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(channeln >= module_list[module_addr].channel_qty || portn >= module_list[module_addr].channels[channeln].port_qty) {
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

int module_get_channel_values(unsigned int module_addr, unsigned int channeln, void **value_ptr) {
	unsigned int port_qty;
	size_t vsize;
	
	if(value_ptr == NULL)
		return -1;
	
	if(module_addr >= MAX_MODULE_QTY) {
		return -3;
	}
	
	if(!xSemaphoreTake(module_manager_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(channeln >= module_list[module_addr].channel_qty) {
		xSemaphoreGive(module_manager_mutex);
		return -3;
	}
	
	port_qty = module_list[module_addr].channels[channeln].port_qty;
	
	if(module_list[module_addr].channels[channeln].type == 'B')
		vsize = sizeof(char);
	else if(module_list[module_addr].channels[channeln].type == 'I')
		vsize = sizeof(int);
	else if(module_list[module_addr].channels[channeln].type == 'F')
		vsize = sizeof(float);
	else {
		*value_ptr = NULL;
		
		xSemaphoreGive(module_manager_mutex);
		
		return -4;
	}
	
	*value_ptr = malloc(vsize * port_qty);
	
	memcpy(*value_ptr, module_list[module_addr].channels[channeln].values, vsize * port_qty);
	
	xSemaphoreGive(module_manager_mutex);
	
	return port_qty;
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
	
	for(mod_addr = 0; mod_addr < MAX_MODULE_QTY; mod_addr++) {
		free_channel_list(module_list[mod_addr].channels, module_list[mod_addr].channel_qty);
		module_list[mod_addr].channel_qty = 0;
	}
	
	/* TODO: retry when communication fail */
	
	for(mod_addr = 0; mod_addr < MAX_MODULE_QTY; mod_addr++) {
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
	
	for(int mod_addr = 0; mod_addr < MAX_MODULE_QTY; mod_addr++)
		module_list[mod_addr].channel_qty = 0;
	
	create_module_list();
	update_values(1);
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
	xTaskNotifyGive(custom_code_task_handle);
	
	while(1) {
		start_time = sdk_system_get_time();
		
		update_values(config_diagnostic_mode);
		
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
