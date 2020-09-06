#include <espressif/esp_common.h>
#include <esp8266.h>
#include <espressif/spi_flash.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include <lwip/sockets.h>
#include <lwip/sys.h>

#include <sysparam.h>

#include "common.h"


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
	socklen_t client_address_size;
	
	int len;
	char *auxptr;
	
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
		client_address_size = sizeof(struct sockaddr_in);
		
		client_socket = accept(main_socket, (struct sockaddr *) &client_address, (socklen_t *) &client_address_size);
		
		if(client_socket < 0)
			continue;
		
		setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
		
		telnet_send_line(client_socket, "Firmware version: "FW_VERSION"");
		telnet_send_line(client_socket, "Build date: "__DATE__" "__TIME__);
		telnet_send_line(client_socket, "Type help for list of commands.");
		telnet_send(client_socket, "> ");
		
		while((len = recv_telnet_line(client_socket, receive_buffer, 200)) >= 0) {
			if(!len) {
				telnet_send(client_socket, "> ");
				continue;
			}
			
			if (receive_buffer[len - 1] == '?') {
				receive_buffer[len - 1] = '\0';
				
				if (sysparam_get_string(receive_buffer, &auxptr) == SYSPARAM_OK) {
					sprintf(send_buffer, "\"%s\" = \"%s\".", receive_buffer, auxptr);
					free(auxptr);
				} else {
					sprintf(send_buffer, "Error reading \"%s\".", receive_buffer);
				}
				
				telnet_send_line(client_socket, send_buffer);
			} else if ((auxptr = strchr(receive_buffer, '='))) {
				*auxptr = '\0';
				auxptr++;
				
				if (sysparam_set_string(receive_buffer, auxptr) == SYSPARAM_OK)
					sprintf(send_buffer, "Value of \"%s\" set to \"%s\".", receive_buffer, auxptr);
				else
					sprintf(send_buffer, "Error setting value of \"%s\".", receive_buffer);
				
				telnet_send_line(client_socket, send_buffer);
			} else if(!strcmp(receive_buffer, "dump")) {
				sysparam_iter_t iterator;
				
				if(sysparam_iter_start(&iterator) != SYSPARAM_OK) {
					telnet_send_line(client_socket, "Error creating iterator!");
					continue;
				}
				
				while (true) {
					if (sysparam_iter_next(&iterator) != SYSPARAM_OK)
						break;
					
					sprintf(send_buffer, "\t\"%s\" = \"%s\"", iterator.key, (iterator.binary) ? "BINARY" : ((char *) iterator.value));
					
					telnet_send_line(client_socket, send_buffer);
				}
				sysparam_iter_end(&iterator);
				
				telnet_send_line(client_socket, "Done.");
			} else if(!strcmp(receive_buffer, "compact")) {
				telnet_send_line(client_socket, "Compacting sysparam data...");
				sysparam_compact();
			} else if(!strcmp(receive_buffer, "help")) {
				telnet_send(client_socket,
					"Available commands:\n"
					" <key>?           -> Query the value of <key>\n"
					" <key>=<value>    -> Set <key> to text <value>\n"
					" dump             -> Show all set keys/values pairs\n"
					" compact          -> Compact sysparam data\n"
					" restart          -> Restart device\r\n"
					" help             -> Show this message\r\n"
				);
				
			} else if(!strcmp(receive_buffer, "restart")) {
				shutdown(client_socket, SHUT_RDWR);
				vTaskDelay(pdMS_TO_TICKS(500));
				close(client_socket);
				vTaskDelay(pdMS_TO_TICKS(500));
				sdk_system_restart();
				
			} else {
				telnet_send_line(client_socket, "Unrecognized command.");
			}
			
			telnet_send(client_socket, "> ");
		}
		
		close(client_socket);
	}
}
