#include <espressif/esp_common.h>
#include <esp8266.h>
#include <espressif/spi_flash.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <i2c/i2c.h>
#include <ds3231/ds3231.h>

#include <FreeRTOS.h>
#include <task.h>

#include <lwip/sockets.h>
#include <lwip/sys.h>

#include <sysparam.h>

#include "common.h"
#include "comm.h"
#include "ota.h"

i2c_dev_t rtc_dev = {.addr = DS3231_ADDR, .bus = 0};

ota_info_t ota_information;

static int recv_telnet_line(int socket_fd, char *buf, size_t len) {
	int num = 0;
	
	do {
		char c;
		
		if(recv(socket_fd, &c, 1, 0) <= 0)
			return -1;
		
		if(c == '\n')
			break;
		
		if(c < 0x20 || c > 0x7e)
			continue;
		
		if(num < len)
			buf[num] = c;
		
		num++;
	} while(1);
	
	buf[(num >= len) ? len - 1 : num] = 0; // Null terminate
	
	return num;
}

static int telnet_send(int socket_fd, const char *buf) {
	return send(socket_fd, buf, strlen(buf), 0);
}

static int telnet_send_line(int socket_fd, const char *buf) {
	char send_buf[205];
	
	sprintf(send_buf, "%s\r\n", buf);
	
	return send(socket_fd, send_buf, strlen(send_buf), 0);
}

void telnet_task(void *pvParameters) {
	int main_socket, client_socket;
	struct sockaddr_in server_bind_address;
	const struct timeval timeout = {5, 0};
	
	struct sockaddr_in client_address;
	socklen_t client_address_size = sizeof(struct sockaddr_in);
	
	int len;
	char aux[50];
	unsigned int addr;
	
	comm_error_t error;
	
	char receive_buffer[200];
	char send_buffer[200];
	
	main_socket = socket(PF_INET, SOCK_STREAM, 0);
	
	bzero(&server_bind_address, sizeof(server_bind_address));
	server_bind_address.sin_family      = AF_INET;
	server_bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_bind_address.sin_port        = htons(23);
	
	bind(main_socket, (struct sockaddr *) &server_bind_address, sizeof(server_bind_address));
	
	listen(main_socket, 2);
	
	while(1) {
		client_socket = accept(main_socket, (struct sockaddr *) &client_address, (socklen_t *) &client_address_size);
		
		if(client_socket < 0)
			continue;
		
		setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
		
		telnet_send_line(client_socket, "Firmware version: "FW_VERSION"");
		telnet_send_line(client_socket, "Build date: "__DATE__" "__TIME__);
		telnet_send_line(client_socket, "Type help for list of commands.");
		
		while((len = recv_telnet_line(client_socket, receive_buffer, 200)) >= 0) {
			if(!len) {
				telnet_send(client_socket, "> ");
				continue;
			}
			
			if(receive_buffer[2] == '?') {
				receive_buffer[2] = '\0';
				
				sscanf(receive_buffer, "%x", &addr);
				
				if(strlen(&receive_buffer[3]) > 31) {
					sprintf(send_buffer, "Content '%s' too long.", &receive_buffer[3]);
				} else {
					comm_send_command('R', (uint8_t) addr, &receive_buffer[3]);
					
					if((error = comm_receive_response('R', aux, 50)))
						sprintf(send_buffer, "Error receiving response! (%s)", comm_error_str(error));
					else if(aux[0] == '\x15')
						sprintf(send_buffer, "Received error: %s", &aux[1]);
					else
						sprintf(send_buffer, "%s", aux);
				}
				
				telnet_send_line(client_socket, send_buffer);
			} else if(receive_buffer[2] == '=') {
				receive_buffer[2] = '\0';
				
				sscanf(receive_buffer, "%x", &addr);
				
				if(strlen(&receive_buffer[3]) > 31) {
					sprintf(send_buffer, "Content '%s' too long.", &receive_buffer[3]);
				} else {
					comm_send_command('W', (uint8_t) addr, &receive_buffer[3]);
					
					if((error = comm_receive_response('W', aux, 50)))
						sprintf(send_buffer, "Error receiving response! (%s)", comm_error_str(error));
					else if(aux[0] == '\x15')
						sprintf(send_buffer, "Received error: %s", &aux[1]);
					else
						sprintf(send_buffer, "OK.", aux);
				}
				
				telnet_send_line(client_socket, send_buffer);
				
			} else if(!strcmp(receive_buffer, "list")) {
				unsigned int qty_channels;
				
				for(addr = 0x00; addr < 0x7F; addr++) {
					comm_send_command('P', (uint8_t) addr, "");
					if((error = comm_receive_response('P', aux, 50)) == COMM_OK) {
						sprintf(send_buffer, "%02X -> %s", addr, aux);
						telnet_send_line(client_socket, send_buffer);
						
						strtok(aux, ":");
						if(sscanf(strtok(NULL, ":"), "%u", &qty_channels) == 1) {
							for(int ch = 0; ch < qty_channels; ch++) {
								sprintf(aux, "%u", ch);
								comm_send_command('D', (uint8_t) addr, aux);
								
								if((error = comm_receive_response('D', aux, 50)) == COMM_OK) {
									telnet_send_line(client_socket, aux);
								} else {
									sprintf(send_buffer, "Error receiving response! (%s)", comm_error_str(error));
									telnet_send_line(client_socket, send_buffer);
								}
							}
						}
					} else if(error != COMM_ERROR_TIMEOUT) {
						sprintf(send_buffer, "%02X -> Error (%s)", addr, comm_error_str(error));
						telnet_send_line(client_socket, send_buffer);
					}
				}
				
			} else if(!strcmp(receive_buffer, "rtc")) {
					struct tm time;
					float tempFloat;
					bool osf;
					
					ds3231_getOscillatorStopFlag(&rtc_dev, &osf);
					ds3231_getTime(&rtc_dev, &time);
					ds3231_getTempFloat(&rtc_dev, &tempFloat);
					
					sprintf(send_buffer, "Temperature: %.1f\nOSF:%c\nTime: %d:%d:%d\nDate: %d-%02d-%02d", tempFloat, (osf ? 'Y' : 'N'),time.tm_hour, time.tm_min, time.tm_sec, time.tm_year + 1900, time.tm_mon + 1, time.tm_mday);
					
					telnet_send_line(client_socket, send_buffer);
					
			} else if(!strncmp(receive_buffer, "ota", 3)) {
				char *hash_ptr;
				
				hash_ptr = strchr(receive_buffer, ' ') + 1;
				
				if(strlen(hash_ptr) != 32) {
					telnet_send_line(client_socket, "<hash> must be an MD5 hash in hexadecimal format.");
					continue;
				}
				
				strcpy(ota_information.server_address, inet_ntoa(client_address.sin_addr));
				strncpy(ota_information.file_hash, hash_ptr, 32);
				
				sprintf(send_buffer, "Connecting to %s for OTA update.", ota_information.server_address);
				telnet_send_line(client_socket, send_buffer);
				
				if(xTaskCreate(ota_task, "ota_task", 1280, (void*) &ota_information, 3, NULL) != pdPASS) {
					telnet_send_line(client_socket, "Failed to create OTA task.\n");
					continue;
				}
				
				close(client_socket);
				
				vTaskDelete(NULL);
			} else if(!strcmp(receive_buffer, "help")) {
				telnet_send(client_socket,
					"Available commands:\n"
					" <addr>?<content> -> Send read command\r\n"
					" <addr>=<content> -> Send write command\r\n"
					" list             -> List connected devices\r\n"
					" ota <hash>       -> Update firmware\r\n"
					" rtc              -> Read RTC data\r\n"
					" restart          -> Restart device\r\n"
					" help             -> Show this message\r\n"
				);
				
			} else if(!strcmp(receive_buffer, "restart")) {
				vTaskDelay(pdMS_TO_TICKS(200));
				close(client_socket);
				vTaskDelay(pdMS_TO_TICKS(1000));
				sdk_system_restart();
				
			} else {
				telnet_send_line(client_socket, "Unrecognized command.");
			}
			
			telnet_send(client_socket, "> ");
		}
		
		close(client_socket);
	}
}
