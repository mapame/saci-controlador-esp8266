typedef enum {
	COMM_OK,
	COMM_ERROR_TIMEOUT,
	COMM_ERROR_NO_START,
	COMM_ERROR_SYNTAX,
	COMM_ERROR_RECEIVE_BUFFER_FULL,
	COMM_ERROR_UNEXPECTED_ADDR,
	COMM_ERROR_UNEXPECTED_OP,
	COMM_ERROR_WRONG_CHECKSUM,
	COMM_ERROR_QTY
} comm_error_t;

const char *comm_error_str(comm_error_t error);
void comm_init();
void comm_send_command(char cmd, unsigned int addr, const char *content);
comm_error_t comm_receive_response(char cmd, char *content_buffer, unsigned int buffer_size);

