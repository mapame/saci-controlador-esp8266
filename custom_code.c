#include <espressif/esp_common.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "common.h"
#include "configuration.h"
#include "module_manager.h"
#include "dashboard.h"

float cc_cycle_duration;

int config_nivel_cisterna_addr;
int_list_t config_nivel_cisterna_ports;

int config_nivel_caixainf_addr;
int_list_t config_nivel_caixainf_ports;

int config_nivel_caixasup_addr;
int_list_t config_nivel_caixasup_ports;

int config_fluxo_rua_addr;
float config_fator_fluxo_rua;

const config_info_t extended_config_table[] = {
	{"nivel_cisterna_addr",		"Sensor de Nível - Cisterna (Endereço)",		'I', "",		0,	32,		1, (void*) &config_nivel_cisterna_addr},
	{"nivel_cisterna_ports",	"Sensor de Nível - Cisterna (Portas)",			'L', "",		0,	255,	1, (void*) &config_nivel_cisterna_ports},
	{"nivel_caixasup_addr",		"Sensor de Nível - Caixa Inf. (Endereço)",		'I', "",		0,	32,		1, (void*) &config_nivel_caixainf_addr},
	{"nivel_caixasup_ports",	"Sensor de Nível - Caixa Inf. (Portas)",		'L', "",		0,	255,	1, (void*) &config_nivel_caixainf_ports},
	{"nivel_caixainf_addr",		"Sensor de Nível - Caixa Sup. (Endereço)",		'I', "",		0,	32,		1, (void*) &config_nivel_caixasup_addr},
	{"nivel_caixainf_ports",	"Sensor de Nível - Caixa Sup. (Portas)",		'L', "",		0,	255,	1, (void*) &config_nivel_caixasup_ports},
	{"fluxo_rua_addr",			"Sensor de Fluxo (Endereço)",					'I', "",		0,	32,		1, (void*) &config_fluxo_rua_addr},
	{"fator_fluxo_rua",			"Sensor de Fluxo: Fator de Conversão",			'F', "1.0",		1,	100,	0, (void*) &config_fator_fluxo_rua},
};

const int extended_config_table_qty = sizeof(extended_config_table) / sizeof(config_info_t);

const char dashboard_line_titles[][64] = {
	"",
	"Controle"
};

const int dashboard_line_title_qty = sizeof(dashboard_line_titles) / (sizeof(char) * 64);

float nivel_cisterna = 0;
char texto_nivel_cisterna[8] = "N/A";
float nivel_caixainf = 0;
char texto_nivel_caixainf[8] = "N/A";
float nivel_caixasup = 0;
char texto_nivel_caixasup[8] = "N/A";
float fluxo_agua;
char texto_fluxo_agua[16] = "N/A";

const dashboard_item_t dashboard_itens[] = {
	{0, {3, 4, 6}, "vertical_gauge",	"Nível Cisterna",			{(void*)&nivel_cisterna, (void*)texto_nivel_cisterna, NULL, NULL}},
	{0, {3, 4, 6}, "vertical_gauge",	"Nível Caixa Inferior",		{(void*)&nivel_caixainf, (void*)texto_nivel_caixainf, NULL, NULL}},
	{0, {3, 4, 6}, "vertical_gauge",	"Nível Caixa Superior",		{(void*)&nivel_caixasup, (void*)texto_nivel_caixasup, NULL, NULL}},
	{0, {3, 4, 6}, "text",				"Fluxo de Água",			{(void*)texto_fluxo_agua, (void*)&(""), NULL, NULL}},
};

const int dashboard_item_qty = sizeof(dashboard_itens) / sizeof(dashboard_item_t);

void custom_code_task(void *pvParameters) {
	
	ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));
	
	if(config_diagnostic_mode) {
		vTaskDelete(NULL);
	}
	
	while(1) {
		
		vTaskDelay(pdMS_TO_TICKS(2000));
	}
	
}
