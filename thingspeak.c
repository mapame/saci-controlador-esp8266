#include "espressif/esp_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>
#include <lwip/api.h>

#include "common.h"
#include "configuration.h"


#define HTTP_BUFFER_SIZE 512

#define THINGSPEAK_HOST "api.thingspeak.com"
#define THINGSPEAK_PORT "80"


char config_thingspeak_ch_key[CONFIG_STR_SIZE];

static const char *http_request_header_format =
	"POST /update HTTP/1.1\r\n"
	"Host: " THINGSPEAK_HOST " \r\n"
	"Content-Type: application/json\r\n"
	"Content-Length: %d\r\n"
	"Connection: close\r\n"
	"THINGSPEAKAPIKEY: %s\r\n"
	"\r\n";

static const int header_max_len = sizeof(http_request_header_format) + 2 + 18 + 1;

static const char *header_termination = "\r\n\r\n";


SemaphoreHandle_t thingspeak_mutex = NULL;

int data_time = 0;
float field_values[8];
char status_text[64];

static int socket_read(int fd, char *buf, size_t len) {
	for (;;) {
		ssize_t rlen;
		
		rlen = read(fd, buf, len);
		if (rlen <= 0) {
			if (rlen < 0 && errno == EINTR)
				continue;
			
			return -1;
		}
		return (int)rlen;
	}
}

static int socket_write(int fd, const char *buf, size_t len) {
	for (;;) {
		ssize_t wlen;
		
		wlen = write(fd, buf, len);
		if (wlen <= 0) {
			if (wlen < 0 && errno == EINTR)
				continue;
			
			return -1;
		}
		return (int)wlen;
	}
}

static int socket_connect(const char *host, const char *port) {
	int sockfd = -1;
	
	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	
	struct addrinfo *res = NULL;
	
	const struct timeval socket_send_timeout = {.tv_sec = 1, .tv_usec = 0};
	const struct timeval socket_receive_timeout = {.tv_sec = 1, .tv_usec = 0};
	
	if(getaddrinfo(host, port, &hints, &res) != 0 || res == NULL)
		return -1;
	
	if((sockfd = socket(res->ai_family, res->ai_socktype, 0)) == -1) {
		freeaddrinfo(res);
		return -1;
	}
	
	if(connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
		close(sockfd);
		freeaddrinfo(res);
		return -1;
	}
	
	freeaddrinfo(res);
	
	setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &socket_send_timeout, sizeof(socket_send_timeout));
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &socket_receive_timeout, sizeof(socket_receive_timeout));
	
	return sockfd;
}

int thingspeak_update(const float *field_ptrs[8], const char *status_ptr) {
	if(thingspeak_mutex == NULL)
		return -1;
	
	if(!xSemaphoreTake(thingspeak_mutex, pdMS_TO_TICKS(200)))
		return -2;
	
	for(int f = 0; f < 8; f++)
		field_values[f] = (field_ptrs[f] == NULL) ? NAN : *field_ptrs[f];
	
	if(status_ptr != NULL) {
		strncpy(status_text, status_ptr, 63);
		status_text[63] = '\0';
	}
	
	data_time = (xTaskGetTickCount() / configTICK_RATE_HZ);
	
	xSemaphoreGive(thingspeak_mutex);
	
	return 0;
}

void thingspeak_task(void *pvParameters) {
	int loaded_data_time;
	int sent_data_time = 0;
	
	char http_buffer[HTTP_BUFFER_SIZE];
	
	int body_len;
	int header_len;
	
	int fd;
	int rlen;
	int first_recvd;
	
	int http_status_code;
	
	int result_state;
	char *result_ptr;
	
	thingspeak_mutex = xSemaphoreCreateMutex();
	
	while(1) {
		vTaskDelay(pdMS_TO_TICKS(500));
		
		if(sdk_wifi_station_get_connect_status() != STATION_GOT_IP || strlen(config_thingspeak_ch_key) < 16)
			continue;
		
		if(data_time == sent_data_time)
			continue;
		
		if(!xSemaphoreTake(thingspeak_mutex, pdMS_TO_TICKS(500)))
			continue;
		
		if((fd = socket_connect(THINGSPEAK_HOST, THINGSPEAK_PORT)) == -1) {
			xSemaphoreGive(thingspeak_mutex);
			continue;
		}
		
		http_buffer[0] = '{';
		body_len = 1;
		
		for(int f = 0; f < 8; f++) {
			if(isnanf(field_values[f]))
				continue;
			
			rlen = snprintf(http_buffer + body_len, (HTTP_BUFFER_SIZE - header_max_len) - body_len, "\"field%d\":%.6f,", f + 1, field_values[f]);
			
			if((body_len + rlen) >= (HTTP_BUFFER_SIZE - header_max_len))
				break;
			
			body_len += rlen;
		}
		
		if(strlen(status_text) > 1) {
			rlen = snprintf(http_buffer + body_len, (HTTP_BUFFER_SIZE - header_max_len) - body_len, "\"status\":\"%s\"", status_text);
			
			if((body_len + rlen + 1) < (HTTP_BUFFER_SIZE - header_max_len))
				body_len += rlen;
		}
		
		loaded_data_time = data_time;
		
		xSemaphoreGive(thingspeak_mutex);
		
		if(http_buffer[body_len - 1] == ',')
			body_len--; // Remove last comma
		
		http_buffer[body_len++] = '}';
		
		header_len = sprintf(http_buffer + body_len + 1, http_request_header_format, body_len, config_thingspeak_ch_key);
		
		if(socket_write(fd, http_buffer + body_len + 1, header_len) <= 0 || socket_write(fd, http_buffer, body_len) <= 0) {
			close(fd);
			continue;
		}
		
		http_status_code = -1;
		first_recvd = 1;
		result_state = 0;
		result_ptr = NULL;
		
		while(1) {
			rlen = socket_read(fd, http_buffer, HTTP_BUFFER_SIZE - 1);
			
			if(rlen <= 0)
				break;
			
			if(first_recvd) {
				first_recvd = 0;
				http_status_code = (int) strtol(http_buffer + 8, NULL, 10);
				
				if(http_status_code != 200)
					break;
			}
			
			for(int i = 0; i < rlen && result_state < 16 + 4; i++)
				if(result_state < 4) {
					result_state = (http_buffer[i] == header_termination[result_state]) ? result_state + 1 : 0;
				} else {
					if(result_state == 4)
						result_ptr = &http_buffer[i];
					
					result_state++;
				}
		}
		
		shutdown(fd, SHUT_RDWR);
		close(fd);
		
		if(result_state > 4)
			result_ptr[result_state - 4] = '\0';
		
		if(http_status_code == 200 && result_state > 4 && ((int) strtol(result_ptr, NULL, 10)) > 0)
			sent_data_time = loaded_data_time;
	}
}
