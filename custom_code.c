#include <espressif/esp_common.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <mqtt_bearssl.h>

#include "common.h"
#include "configuration.h"
#include "module_manager.h"
#include "dashboard.h"
#include "mqtt_task.h"
#define COMMAND_QUEUE_DEPTH 10
#define COMMAND_MAX_LEN 32

QueueHandle_t cc_command_queue = NULL;
float cc_cycle_duration;

float config_teste;
char config_teste_texto[CONFIG_STR_SIZE];

const config_info_t extended_config_table[] = {
	{"teste_configuracao",		"Configuração de Teste",						'F', "",		0,	100,	0, (void*) &config_teste},
	{"teste_configuracao_texto","Configuração de Teste (Texto)",				'T', "",		0,	20,		0, (void*) &config_teste_texto}
};

const int extended_config_table_qty = sizeof(extended_config_table) / sizeof(config_info_t);

const char dashboard_line_titles[][64] = {
	"",
	"Textos",
	"Botões"
};

const int dashboard_line_title_qty = sizeof(dashboard_line_titles) / (sizeof(char) * 64);

float gauge_teste_value = 0;
char gauge_teste_text[8] = "0.0 %";
char text_teste_text1[16] = "0 sec";
char text_teste_text2[32] = "Hello World!";


const dashboard_item_t dashboard_itens[] = {
	{0, {4, 6, 12},	"vertical_gauge",	"Memória Heap Livre",		{(void*)&gauge_teste_value, (void*)gauge_teste_text, NULL, NULL}},
	{0, {4, 6, 12},	"vertical_gauge",	"Teste Indicador",			{(void*)&config_teste, (void*)&("Teste"), NULL, NULL}},
	{1, {4, 6, 12},	"text",				"Uptime",					{(void*)text_teste_text1, (void*)&(""), NULL, NULL}},
	{1, {4, 6, 12},	"text",				"Teste Texto",				{(void*)text_teste_text2, (void*)&(""), NULL, NULL}},
	{2, {4, 6, 12},	"button",			"Botão 1",					{(void*)&((int){1}), (void*)&("CMD_TESTE1"), NULL, NULL}},
	{2, {4, 6, 12},	"button",			"Botão 2",					{(void*)&((int){1}), (void*)&("CMD_TESTE2"), NULL, NULL}}
};

const int dashboard_item_qty = sizeof(dashboard_itens) / sizeof(dashboard_item_t);

void custom_code_task(void *pvParameters) {
	int counter = 0;
	
	uint32_t start_time, end_time;
	int cycle_duration[3], cycle_count = 0;
	int time_to_wait;
	char command[32];
	
	
	cc_command_queue = xQueueCreate(COMMAND_QUEUE_DEPTH, COMMAND_MAX_LEN * sizeof(char));
	
	ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(60*1000));
	
	if(config_diagnostic_mode) {
		vTaskDelete(NULL);
	}
	
	while(1) {
		start_time = sdk_system_get_time();
		
		gauge_teste_value = (((float)xPortGetFreeHeapSize()) / 81920.0) * 100.0;
		snprintf(gauge_teste_text, sizeof(gauge_teste_text), "%.1f %%", gauge_teste_value);
		
		snprintf(text_teste_text1, sizeof(text_teste_text1), "%u secs", (xTaskGetTickCount() * portTICK_PERIOD_MS / 1000));
		
		while (xQueueReceive(cc_command_queue, (void *)command, 0) == pdPASS) {
			if(!strncmp(command, "CMD_TESTE1", 32)) {
				strcpy(text_teste_text2, "Botão 1 pressionado");
			} else if(!strncmp(command, "CMD_TESTE2", 32)) {
				strcpy(text_teste_text2, "Botão 2 pressionado");
			}
		}
		
		counter = (counter + 1) % 30;
		
		end_time = sdk_system_get_time();
		
		cycle_count = (cycle_count + 1) % 3;
		cycle_duration[cycle_count] = SYSTIME_DIFF(start_time, end_time) / 1000U;
		cc_cycle_duration = ((float)(cycle_duration[0] + cycle_duration[1] + cycle_duration[2])) / 3.0;
		
		time_to_wait = 2000 - cycle_duration[cycle_count];
		
		vTaskDelay(pdMS_TO_TICKS(MAX(time_to_wait, 200)));
	}
	
}
