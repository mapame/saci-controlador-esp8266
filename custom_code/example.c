#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <espressif/esp_wifi.h>

#include <FreeRTOS.h>
#include <task.h>

#include "common.h"
#include "configuration.h"
#include "module_manager.h"
#include "dashboard.h"
#include "mqtt_task.h"
#include "thingspeak.h"


const char custom_code_version[] = "example_2";

float config_teste;
char config_teste_texto[CONFIG_STR_SIZE];

const config_info_t extended_config_table[] = {
	{"teste_configuracao",		"Configuração de Teste",						'F', "",		0,	100,	0, (void*) &config_teste},
	{"teste_configuracao_texto","Configuração de Teste (Texto)",				'T', "",		0,	20,		0, (void*) &config_teste_texto}
};

const char dashboard_line_titles[][64] = {
	"",
	"Textos",
	"Botões"
};

float gauge_teste_value = 0;
char gauge_teste_text[8] = "0.0 %";
char text_teste_text1[16] = "0 sec";
char text_teste_text2[32] = "Hello World!";
char text_ts_status[64];

const float *thingspeak_values[8] = {&gauge_teste_value, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

const dashboard_item_t dashboard_itens[] = {
	{0, {4, 6, 12},	"vertical_gauge",	"Memória Heap Livre",		{(void*)&gauge_teste_value, (void*)gauge_teste_text, NULL, NULL}},
	{0, {4, 6, 12},	"vertical_gauge",	"Teste Indicador",			{(void*)&config_teste, (void*)&("Teste"), NULL, NULL}},
	{1, {4, 6, 12},	"text",				"Uptime",					{(void*)text_teste_text1, (void*)&(""), NULL, NULL}},
	{1, {4, 6, 12},	"text",				"Teste Texto",				{(void*)text_teste_text2, (void*)&(""), NULL, NULL}},
	{2, {4, 6, 12},	"button",			"Botão 1",					{(void*)&((int){1}), (void*)&("CMD_TESTE1"), NULL, NULL}},
	{2, {4, 6, 12},	"button",			"Botão 2",					{(void*)&((int){1}), (void*)&("CMD_TESTE2"), NULL, NULL}}
};

const int extended_config_table_qty = sizeof(extended_config_table) / sizeof(config_info_t);
const int dashboard_line_title_qty = sizeof(dashboard_line_titles) / (sizeof(char) * 64);
const int dashboard_item_qty = sizeof(dashboard_itens) / sizeof(dashboard_item_t);


int custom_code_setup() {
	return 0;
}

int custom_code_command(const char *cmd, size_t cmd_len) {
	if(!strncmp(cmd, "CMD_TESTE1", cmd_len)) {
		strcpy(text_teste_text2, "Botão 1 pressionado");
	} else if(!strncmp(cmd, "CMD_TESTE2", cmd_len)) {
		strcpy(text_teste_text2, "Botão 2 pressionado");
	}
	
	return 0;
}

int custom_code_loop(int counter, time_t rtc_time) {
	gauge_teste_value = (((float)xPortGetFreeHeapSize()) / 81920.0) * 100.0;
	snprintf(gauge_teste_text, sizeof(gauge_teste_text), "%.1f %%", gauge_teste_value);
	
	snprintf(text_teste_text1, sizeof(text_teste_text1), "%u secs", (xTaskGetTickCount() * portTICK_PERIOD_MS / 1000));
	
	if(counter == 0 || counter == 15) {
		struct ip_info station_ip_info;
		char ip_str[IP4ADDR_STRLEN_MAX];
		
		mqtt_task_publish_float("heap", gauge_teste_value, 0, 0);
		
		sdk_wifi_get_ip_info(STATION_IF, &station_ip_info);
		snprintf(text_ts_status, sizeof(text_ts_status), "IP: %s | Uptime: %u s", ip4addr_ntoa_r(&station_ip_info.ip, ip_str, IP4ADDR_STRLEN_MAX), (xTaskGetTickCount() * portTICK_PERIOD_MS / 1000));
		
		thingspeak_update(thingspeak_values, text_ts_status);
	}
	
	return 0;
}
