#ifndef HTTP_OTA_H
#define HTTP_OTA_H

typedef enum {
	OTA_UPDATE_DONE = 0,
	OTA_HASH_DONT_MATCH,
	OTA_ONE_SLOT_ONLY,
	OTA_FAIL_SET_NEW_SLOT,
	OTA_IMAGE_VERIFY_FAILED
} OTA_err;

typedef struct {
	const char *server;
	const char *port;
	const char *path;
	const char *hash_str;
} OTA_config_t;

const char* ota_err_str(int code);
OTA_err ota_update(OTA_config_t *ota_config);
#endif // ifndef HTTP_OTA_H
