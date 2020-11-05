#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sysparam.h>

#include "common.h"
#include "configuration.h"


extern int config_diagnostic_mode;

extern char config_webui_password[CONFIG_STR_SIZE];

extern char config_wifi_ssid[CONFIG_STR_SIZE];
extern char config_wifi_password[CONFIG_STR_SIZE];
extern char config_wifi_ap_password[CONFIG_STR_SIZE];

extern int config_mqtt_enabled;
extern char config_mqtt_username[CONFIG_STR_SIZE];
extern char config_mqtt_password[CONFIG_STR_SIZE];
extern char config_mqtt_clientid[CONFIG_STR_SIZE];
extern char config_mqtt_topic_prefix[CONFIG_STR_SIZE];


const config_info_t base_config_table[] = {
	{"diagnostic_mode",			"", 'B', "1",				0,  1, 1, (void*) &config_diagnostic_mode},
	{"webui_password",			"", 'T', "1234SACI",		8, 32, 0, (void*) &config_webui_password},
	{"wifi_ssid",				"", 'T', "",				1, 32, 1, (void*) &config_wifi_ssid},
	{"wifi_password",			"", 'T', "",				1, 63, 1, (void*) &config_wifi_password},
	{"wifi_ap_password",		"", 'T', "1234SACI",		8, 32, 1, (void*) &config_wifi_ap_password},
	{"mqtt_enabled",			"", 'B', "0",				0,  1, 1, (void*) &config_mqtt_enabled},
	{"mqtt_username",			"", 'T', "",				0, 63, 1, (void*) &config_mqtt_username},
	{"mqtt_password",			"", 'T', "",				0, 63, 1, (void*) &config_mqtt_password},
	{"mqtt_clientid",			"", 'T', "",				0, 63, 1, (void*) &config_mqtt_clientid},
	{"mqtt_topic_prefix",		"", 'T', "",				0, 63, 1, (void*) &config_mqtt_topic_prefix},
};

const int base_config_table_qty = sizeof(base_config_table) / sizeof(config_info_t);

extern const config_info_t extended_config_table[];


static int convert_text_to_int_list(const char *text, int min, int max, int_list_t *list) {
	char auxt[128];
	char *saveptr = NULL;
	char *tokptr = NULL;
	int auxi;
	
	if(text == NULL || list == NULL)
		return -1;
	
	list->qty = 0;
	
	if(text[0] == '\0') {
		list->values = NULL;
		return -2;
	}
	
	list->values = (int*) malloc(10 * sizeof(int));
	
	strncpy(auxt, text, 127);
	auxt[127] = '\0';
	
	tokptr = strtok_r(auxt, ",", &saveptr);
	
	do {
		if(list->qty >= 10)
			list->values = (int*) realloc(list->values, list->qty * sizeof(int));
		
		if(sscanf(tokptr, "%d", &auxi) != 1) {
			free(list->values);
			list->values = NULL;
			return -3;
		}
		
		if(auxi > max || auxi < min) {
			free(list->values);
			list->values = NULL;
			return -4;
		}
		
		list->values[list->qty++] = auxi;
	} while((tokptr = strtok_r(NULL, ",", &saveptr)) != NULL);
	
	list->values = (int*) realloc(list->values, list->qty * sizeof(int));
	
	return list->qty;
}

int configuration_get_index(const char *name) {
	if(name == NULL)
		return -1;
	
	for(int i = 0; i < base_config_table_qty; i++)
		if(!strcmp(name, base_config_table[i].name))
			return i;
	
	for(int i = 0; i < extended_config_table_qty; i++)
		if(!strcmp(name, extended_config_table[i].name))
			return i + base_config_table_qty;
	
	return -2;
}

int configuration_get_info(unsigned int index, const config_info_t **info_ptr) {
	if(index >= base_config_table_qty + extended_config_table_qty)
		return -1;
	
	if(info_ptr == NULL)
		return -2;
	
	if(index < base_config_table_qty)
		*info_ptr = &(base_config_table[index]);
	else
		*info_ptr = &(extended_config_table[index - base_config_table_qty]);
	
	return 0;
}

int configuration_read_value(unsigned int index, char *buffer, unsigned int buffer_len) {
	const config_info_t *config_ptr;
	char *value_ptr = NULL;
	sysparam_status_t status;
	
	if(index >= base_config_table_qty + extended_config_table_qty)
		return -1;
	
	if(buffer == NULL)
		return -2;
	
	if(buffer_len < 1)
		return -3;
	
	if(index < base_config_table_qty)
		config_ptr = &(base_config_table[index]);
	else
		config_ptr = &(extended_config_table[index - base_config_table_qty]);
	
	status = sysparam_get_string(config_ptr->name, &value_ptr);
	
	
	if(status == SYSPARAM_OK) {
		strncpy(buffer, (const char*) value_ptr, buffer_len);
		
		free(value_ptr);
		
	} else if(status == SYSPARAM_NOTFOUND || status == SYSPARAM_PARSEFAILED) {
		strncpy(buffer, (const char*) config_ptr->default_value, buffer_len);
	} else {
		return -4;
	}
	
	buffer[buffer_len - 1] = '\0';
	
	return 0;
}

int configuration_write_value(unsigned int index, const char *buffer) {
	const config_info_t *config_ptr;
	int tmp_i;
	float tmp_f;
	int_list_t tmp_l;
	
	if(index >= base_config_table_qty + extended_config_table_qty)
		return -1;
	
	if(buffer == NULL)
		return -2;
	
	if(index < base_config_table_qty)
		config_ptr = &(base_config_table[index]);
	else
		config_ptr = &(extended_config_table[index - base_config_table_qty]);
	
	switch(config_ptr->type) {
		case 'L':
			if(strlen(buffer) >= 128)
				return -5;
			
			if(convert_text_to_int_list(buffer, (int)config_ptr->min, (int)config_ptr->max, &tmp_l) <= 0)
				return -4;
			
			break;
		case 'T':
			if(strlen(buffer) > MIN(config_ptr->max, CONFIG_STR_SIZE) || strlen(buffer) < config_ptr->min)
				return -5;
			
			break;
		case 'B':
			if(buffer[0] != '0' && buffer[0] != '1')
				return -4;
			
			tmp_i = (buffer[0] == '0') ? 0 : 1;
			
			break;
		case 'I':
			if(sscanf(buffer, "%d", &tmp_i) != 1)
				return -4;
			
			if(((float) tmp_i) > config_ptr->max || ((float) tmp_i) < config_ptr->min)
				return -5;
			
			break;
		case 'F':
			if(sscanf(buffer, "%f", &tmp_f) != 1)
				return -4;
			
			if(tmp_f > config_ptr->max || tmp_f < config_ptr->min)
				return -5;
			
			break;
	}
	
	if(sysparam_set_string(config_ptr->name, buffer) != SYSPARAM_OK)
		return -6;
	
	if(config_ptr->require_restart)
		return 0;
	
	switch(config_ptr->type) {
		case 'L':
			free(((int_list_t*)config_ptr->variable_ptr)->values);
			memcpy(config_ptr->variable_ptr, &tmp_l, sizeof(int_list_t));
			
			break;
		case 'T':
			strncpy((char*) config_ptr->variable_ptr, (const char*) buffer, CONFIG_STR_SIZE);
			((char*) config_ptr->variable_ptr)[CONFIG_STR_SIZE - 1] = '\0';
			
			break;
		case 'B':
		case 'I':
			*((int*) config_ptr->variable_ptr) = tmp_i;
			
			break;
		case 'F':
			*((float*) config_ptr->variable_ptr) = tmp_f;
			
			break;
	}
	
	return 0;
}

void configuration_load() {
	const config_info_t *config_ptr;
	char *value_ptr;
	sysparam_status_t status;
	
	for(int index = 0; index < (base_config_table_qty + extended_config_table_qty); index++) {
		if(index < base_config_table_qty)
			config_ptr = &(base_config_table[index]);
		else
			config_ptr = &(extended_config_table[index - base_config_table_qty]);
		
		value_ptr = NULL;
		
		status = sysparam_get_string(config_ptr->name, &value_ptr);
		
		switch(config_ptr->type) {
			case 'L':
				if(status != SYSPARAM_OK || convert_text_to_int_list(value_ptr, (int)config_ptr->min, (int)config_ptr->max, (int_list_t*)config_ptr->variable_ptr) <= 0)
					convert_text_to_int_list(config_ptr->default_value, (int)config_ptr->min, (int)config_ptr->max, (int_list_t*)config_ptr->variable_ptr);
				
				break;
			case 'T':
				if(status != SYSPARAM_OK)
					strncpy((char*) config_ptr->variable_ptr, config_ptr->default_value, CONFIG_STR_SIZE);
				else
					strncpy((char*) config_ptr->variable_ptr, (const char*) value_ptr, CONFIG_STR_SIZE);
				
				((char*) config_ptr->variable_ptr)[CONFIG_STR_SIZE - 1] = '\0';
				
				break;
			case 'B':
				if(status != SYSPARAM_OK || (value_ptr[0] != '1' && value_ptr[0] != '0'))
					*((int*) config_ptr->variable_ptr) = (config_ptr->default_value[0] == '0') ? 0 : 1;
				else
					*((int*) config_ptr->variable_ptr) = (value_ptr[0] == '0') ? 0 : 1;
				
				break;
			case 'I':
				if(status != SYSPARAM_OK || (sscanf(value_ptr, "%d", (int*) config_ptr->variable_ptr) != 1))
					sscanf(config_ptr->default_value, "%d", (int*) config_ptr->variable_ptr);
				
				break;
			case 'F':
				if(status != SYSPARAM_OK || (sscanf(value_ptr, "%f", (float*) config_ptr->variable_ptr) != 1))
					sscanf(config_ptr->default_value, "%f", (float*) config_ptr->variable_ptr);
				
				break;
		}
		
		free(value_ptr);
	}
	
}

void configuration_cleanup() {
	sysparam_iter_t iterator;
	
	if(sysparam_iter_start(&iterator) != SYSPARAM_OK)
		return;
	
	while(1) {
		if(sysparam_iter_next(&iterator) != SYSPARAM_OK)
			break;
		
		if(configuration_get_index(iterator.key) < 0)
			sysparam_set_data(iterator.key, NULL, 0, 0);
	}
	sysparam_iter_end(&iterator);
}

