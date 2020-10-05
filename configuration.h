#define CONFIG_STR_SIZE 64
#define CONFIG_TABLE_TOTAL_QTY (base_config_table_qty + extended_config_table_qty)

typedef struct config_info_s {
	char name[32];
	char fname[64];
	char type;
	char default_value[CONFIG_STR_SIZE];
	float min;
	float max;
	int require_restart;
	void *variable_ptr;
} config_info_t;

extern const int base_config_table_qty;
extern const config_info_t base_config_table[];

extern const int extended_config_table_qty;
extern const config_info_t extended_config_table[];

extern int config_diagnostic_mode;

extern char config_webui_password[CONFIG_STR_SIZE];
extern char config_wifi_ssid[CONFIG_STR_SIZE];
extern char config_wifi_password[CONFIG_STR_SIZE];
extern char config_wifi_ap_password[CONFIG_STR_SIZE];
extern char config_thingspeak_channel_id[CONFIG_STR_SIZE];
extern char config_thingspeak_channel_key[CONFIG_STR_SIZE];
extern char config_telegram_bot_token[CONFIG_STR_SIZE];
extern char config_telegram_group_id[CONFIG_STR_SIZE];


int configuration_get_index(const char *name);
int configuration_get_info(unsigned int index, const config_info_t **info_ptr);
int configuration_read_value(unsigned int index, char *buffer, unsigned int buffer_len);
int configuration_write_value(unsigned int index, const char *buffer);
void configuration_load();

