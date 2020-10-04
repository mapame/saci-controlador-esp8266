#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sysparam.h>

#include "common.h"
#include "configuration.h"


int config_diagnose_mode;

char config_webui_password[CONFIG_STR_SIZE];
char config_wifi_ssid[CONFIG_STR_SIZE];
char config_wifi_password[CONFIG_STR_SIZE];
char config_wifi_ap_password[CONFIG_STR_SIZE];
char config_thingspeak_channel_id[CONFIG_STR_SIZE];
char config_thingspeak_channel_key[CONFIG_STR_SIZE];
char config_telegram_bot_token[CONFIG_STR_SIZE];
char config_telegram_group_id[CONFIG_STR_SIZE];

const int base_config_table_qty = 9;

const config_info_t base_config_table[] = {
	{"diagnose_mode",			"", 'B', "1",				0,  1, 1, (void*) &config_diagnose_mode},
	{"webui_password",			"", 'T', "1234SACI",		8, 32, 0, (void*) &config_webui_password},
	{"wifi_ssid",				"", 'T', "",				1, 32, 1, (void*) &config_wifi_ssid},
	{"wifi_password",			"", 'T', "",				1, 63, 1, (void*) &config_wifi_password},
	{"wifi_ap_password",		"", 'T', "1234SACI",		8, 32, 1, (void*) &config_wifi_ap_password},
	{"thingspeak_channel_id",	"", 'T', "",				0, 63, 0, (void*) &config_thingspeak_channel_id},
	{"thingspeak_channel_key",	"", 'T', "",				0, 63, 0, (void*) &config_thingspeak_channel_key},
	{"telegram_bot_token",		"", 'T', "",				0, 63, 0, (void*) &config_telegram_bot_token},
	{"telegram_group_id",		"", 'T', "",				0, 63, 0, (void*) &config_telegram_group_id},
};

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
	
	if(index >= base_config_table_qty + extended_config_table_qty)
		return -1;
	
	if(buffer == NULL)
		return -2;
	
	if(index < base_config_table_qty)
		config_ptr = &(base_config_table[index]);
	else
		config_ptr = &(extended_config_table[index - base_config_table_qty]);
	
	switch(config_ptr->type) {
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

