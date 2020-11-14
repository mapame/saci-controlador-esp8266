#include "espressif/esp_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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


int config_thingspeak_enabled;
char config_thingspeak_ch_key[CONFIG_STR_SIZE];

static const char *http_request_header_format =
	"POST /update HTTP/1.1\r\n"
	"Host: " THINGSPEAK_HOST " \r\n"
	"Content-Type: application/json\r\n"
	"Content-Length: %d\r\n"
	"Connection: close\r\n"
	"THINGSPEAKAPIKEY: %s\r\n"
	"\r\n";

static const int header_max_len = sizeof(http_request_header_format) + 2 + 16 + 1;

static const char *header_termination = "\r\n\r\n";


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

int thingspeak_update(const float *fields[8], const char *status) {
	char *http_buffer = NULL;
	
	int body_len;
	int header_len;
	
	int fd;
	int rlen;
	int first_recvd = 1;
	
	int http_status_code = -1;
	
	int result_state = 0;
	char result_str[16];
	int result_value;
	
	if(sdk_wifi_station_get_connect_status() != STATION_GOT_IP)
		return -1;
	
	if(config_thingspeak_enabled == 0 || strlen(config_thingspeak_ch_key) < 16)
		return -2;
	
	if((http_buffer = (char*) malloc(HTTP_BUFFER_SIZE * sizeof(char))) == NULL) {
		free(http_buffer);
		return -3;
	}
	
	if((fd = socket_connect(THINGSPEAK_HOST, THINGSPEAK_PORT)) == -1) {
		free(http_buffer);
		return -4;
	}
	
	http_buffer[0] = '{';
	body_len = 1;
	
	for(int f = 0; f < 8; f++) {
		if(fields[f] == NULL)
			continue;
		
		rlen = snprintf(http_buffer + body_len, (HTTP_BUFFER_SIZE - header_max_len) - body_len, "\"field%d\":%.6f,", f + 1, *fields[f]);
		
		if((body_len + rlen) >= (HTTP_BUFFER_SIZE - header_max_len))
			break;
		
		body_len += rlen;
	}
	
	if(status) {
		rlen = snprintf(http_buffer + body_len, (HTTP_BUFFER_SIZE - header_max_len) - body_len, "\"status\":\"%s\"", status);
		
		if((body_len + rlen + 1) < (HTTP_BUFFER_SIZE - header_max_len))
			body_len += rlen;
	}
	
	if(http_buffer[body_len - 1] == ',')
		body_len--; // Remove last comma
	
	http_buffer[body_len++] = '}';
	
	header_len = sprintf(http_buffer + body_len + 1, http_request_header_format, body_len, config_thingspeak_ch_key);
	
	if(socket_write(fd, http_buffer + body_len + 1, header_len) <= 0 || socket_write(fd, http_buffer, body_len) <= 0) {
		close(fd);
		free(http_buffer);
		return -5;
	}
	
	while(1) {
		rlen = socket_read(fd, http_buffer, HTTP_BUFFER_SIZE - 1);
		
		if(rlen <= 0)
			break;
		
		if(first_recvd) {
			first_recvd = 0;
			http_status_code = (int) strtol(http_buffer + 8, NULL, 10);
		}
		
		for(int i = 0; i < rlen && result_state < (16 + 4 - 1); i++) {
			if(result_state < 4)
				result_state = (http_buffer[i] == header_termination[result_state]) ? result_state + 1 : 0;
			else
				result_str[(result_state++) - 4] = http_buffer[i];
		}
	}
	
	free(http_buffer);
	
	shutdown(fd, SHUT_RDWR);
	close(fd);
	
	if(http_status_code != 200)
		return -http_status_code;
	
	if(result_state < 5)
		return -6;
	
	result_str[result_state - 4] = '\0';
	
	if((result_value = (int) strtol(result_str, NULL, 10)) <= 0)
		return -7;
	
	return result_value;
}
