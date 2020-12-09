#include <espressif/esp_common.h>
#include <esp8266.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <semphr.h>

#include <i2c/i2c.h>
#include <ds3231/ds3231.h>

#include "rtc.h"

i2c_dev_t rtc_dev = {.addr = DS3231_ADDR, .bus = 0};

SemaphoreHandle_t rtc_mutex = NULL;

float config_time_zone;

void rtc_init() {
	rtc_mutex = xSemaphoreCreateMutex();
	
	return;
}

int rtc_get_time(time_t *time) {
	struct tm bdtime;
	bool osf;
	
	if(time == NULL)
		return -1;
	
	if(!xSemaphoreTake(rtc_mutex, pdMS_TO_TICKS(500)))
		return -2;
	
	if(!ds3231_getOscillatorStopFlag(&rtc_dev, &osf)) {
		xSemaphoreGive(rtc_mutex);
		
		return -3;
	}
	
	if(osf) {
		xSemaphoreGive(rtc_mutex);
		
		*time = 0;
		
		return -4;
	}
	
	if(!ds3231_getTime(&rtc_dev, &bdtime)) {
		xSemaphoreGive(rtc_mutex);
		
		return -3;
	}
	
	*time = mktime(&bdtime);
	
	xSemaphoreGive(rtc_mutex);
	
	return 0;
}

int rtc_get_time_local(time_t *time) {
	int result;
	
	result = rtc_get_time(time);
	
	if(result < 0)
		return result;
	
	*time += (int)(config_time_zone * 3600.0);
	
	return 0;
}

int rtc_get_temp(float *temp) {
	if(temp == NULL)
		return -1;
	
	if(!xSemaphoreTake(rtc_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(!ds3231_getTempFloat(&rtc_dev, temp)) {
		xSemaphoreGive(rtc_mutex);
		return -3;
	}
	
	xSemaphoreGive(rtc_mutex);
	
	return 0;
}

int rtc_set_time(const time_t time) {
	struct tm new_time_tm;
	
	gmtime_r(&time, &new_time_tm);
	
	if(!xSemaphoreTake(rtc_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	if(!ds3231_clearOscillatorStopFlag(&rtc_dev)) {
		xSemaphoreGive(rtc_mutex);
		
		return -3;
	}
	
	if(ds3231_setTime(&rtc_dev, &new_time_tm)) {
		xSemaphoreGive(rtc_mutex);
		
		return -3;
	}
	
	xSemaphoreGive(rtc_mutex);
	
	return 0;
}
