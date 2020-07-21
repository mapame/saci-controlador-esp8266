#include <stdio.h>
#include <unistd.h>

#include <espressif/esp_misc.h>
#include <esp/uart.h>

#include <FreeRTOS.h>

#include "common.h"
#include "crc16.h"
#include "comm.h"

const char error_str_table[COMM_ERROR_QTY][32] = {
	"COMM_OK",
	"COMM_ERROR_TIMEOUT",
	"COMM_ERROR_NO_START",
	"COMM_ERROR_SYNTAX",
	"COMM_ERROR_RECEIVE_BUFFER_FULL",
	"COMM_ERROR_UNEXPECTED_ADDR",
	"COMM_ERROR_UNEXPECTED_OP",
	"COMM_ERROR_WRONG_CHECKSUM"
};

const char *comm_error_str(comm_error_t error) {
	return error_str_table[error];
}

void comm_init() {
	uart_set_baud(0, BAUDRATE);
	uart_set_baud(1, BAUDRATE);
	
	/* Activate UART for GPIO2 */
	gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
	
	gpio_enable(DE_PIN, GPIO_OUTPUT);
	gpio_write(DE_PIN, 0);
	
	uart_set_stopbits(1, UART_STOPBITS_1);
	
	uart_set_parity_enabled(1, false);
	
	uart_clear_rxfifo(0);
}

void comm_send_command(char cmd, unsigned int addr, const char *content) {
	char tx_buffer[64];
	size_t tx_size;
	uint16_t checksum;
	
	tx_size = (size_t) sprintf(tx_buffer, "\x01""%02X:%c""\x02""%s""\x03", addr, cmd, content);
	
	checksum = crc16_calculate((uint8_t*)tx_buffer, tx_size);
	
	sprintf(&tx_buffer[tx_size], "%04X", checksum);
	
	uart_clear_rxfifo(0);
	uart_flush_txfifo(1);
	
	gpio_write(DE_PIN, 1);
	
	sdk_os_delay_us(50);
	
	for(int pos = 0; pos < (tx_size + 4); pos++) {
		uart_putc(1,*(tx_buffer + pos));
	}
	
	uart_flush_txfifo(1);
	
	sdk_os_delay_us((9000000 / BAUDRATE) + 50);
	
	gpio_write(DE_PIN, 0);
}

comm_error_t comm_receive_response(char cmd, char *content_buffer, unsigned int buffer_size) {
	int received;
	char c;
	int state = 0;
	
	int timeout_retries;
	int start_retries = 4;
	
	char rx_buffer[64];
	unsigned int buf_len = 0;
	
	unsigned int content_len = 0;
	
	char checksum[5];
	
	while(state <= 10) {
		/* TODO: check CPU frequency and set the appropriate value */
		/* At 80 MHz each cycle takes ~250 ns */
		timeout_retries = 20000;
		
		while((received = uart_getc_nowait(0)) == -1) { /* Read 1 byte from uart0 */
				timeout_retries--;
				
				if(timeout_retries == 0)
					return COMM_ERROR_TIMEOUT;
		}
		
		c = (char) (received & 0x000000ff);
		
		if(state <= 6)
			rx_buffer[buf_len++] = c;
		
		switch(state) {
			case 0:
				if(c == '\x01') /* SOH */
					state++;
				else if(start_retries--)
					buf_len = 0;
				else
					return COMM_ERROR_NO_START;
				
				break;
			case 1:
			case 2:
				if(c == 'F')
					state++;
				else {
					return COMM_ERROR_UNEXPECTED_ADDR;
				}
				
				break;
			case 3:
				if(c == ':')
					state++;
				else
					return COMM_ERROR_SYNTAX;
				break;
			case 4:
				if(c == cmd)
					state++;
				else
					return COMM_ERROR_UNEXPECTED_OP;
				
				break;
			case 5:
				if(c == '\x02') /* STX */
					state++;
				else
					return COMM_ERROR_SYNTAX;
				
				break;
			case 6:
				if(c == '\x03') { /* ETX */
					state++;
					sprintf(checksum, "%04X", crc16_calculate((uint8_t*)rx_buffer, buf_len));
					content_buffer[content_len] = '\0';
					break;
				}
				
				/* Return error if the rx buffer is full and the received byte is not ETX */
				if(buf_len >= 64)
					return COMM_ERROR_RECEIVE_BUFFER_FULL;
				
				if(content_len < (buffer_size - 1))
					content_buffer[content_len++] = c;
				
				break;
			case 7:
			case 8:
			case 9:
			case 10:
				if(c == checksum[state - 7])
					state++;
				else
					return COMM_ERROR_WRONG_CHECKSUM;
				break;
			default:
				break;
		}
	}
	
	return COMM_OK;
}
