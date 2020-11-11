#include <espressif/esp_common.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <mqtt_bearssl.h>

#include "common.h"
#include "configuration.h"
#include "module_manager.h"
#include "rtc.h"
#include "dashboard.h"
#include "mqtt_task.h"

#define COMMAND_QUEUE_DEPTH 10
#define COMMAND_MAX_LEN 32

QueueHandle_t cc_command_queue = NULL;
float cc_cycle_duration;

int custom_code_setup();
int custom_code_command(const char *cmd, size_t cmd_len);
int custom_code_loop(int counter, time_t rtc_time);

void custom_code_task(void *pvParameters) {
	int counter = 0;
	time_t rtc_time;
	
	uint32_t start_time, end_time;
	int cycle_duration[3], cycle_count = 0;
	int time_to_wait;
	char command[COMMAND_MAX_LEN];
	
	
	cc_command_queue = xQueueCreate(COMMAND_QUEUE_DEPTH, COMMAND_MAX_LEN * sizeof(char));
	
	ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(60*1000));
	
	if(config_diagnostic_mode) {
		vTaskDelete(NULL);
	}
	
	custom_code_setup();
	
	while(1) {
		start_time = sdk_system_get_time();
		
		rtc_get_time(&rtc_time);
		
		custom_code_loop(counter, rtc_time);
		
		if(xQueueReceive(cc_command_queue, (void *)command, 0) == pdPASS)
			custom_code_command(command, sizeof(command));
		
		end_time = sdk_system_get_time();
		
		cycle_count = (cycle_count + 1) % 3;
		cycle_duration[cycle_count] = SYSTIME_DIFF(start_time, end_time) / 1000U;
		cc_cycle_duration = ((float)(cycle_duration[0] + cycle_duration[1] + cycle_duration[2])) / 3.0;
		
		time_to_wait = 2000 - cycle_duration[cycle_count];
		
		vTaskDelay(pdMS_TO_TICKS(MAX(time_to_wait, 200)));
		
		counter = (counter + 1) % 30;
	}
	
}
