typedef struct {
	char server_address[32];
	char file_hash[33];
} ota_info_t;

void ota_task(void *pvParameters);
