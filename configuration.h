#define CONFIG_STR_SIZE 64
#define CONFIG_TABLE_TOTAL_QTY (base_config_table_qty + extended_config_table_qty)

typedef struct config_info_s {
	int formn;
	char name[32];
	char dname[64];
	char type;
	char default_value[CONFIG_STR_SIZE];
	float min;
	float max;
	int require_restart;
	void *variable_ptr;
} config_info_t;

typedef struct int_list_s {
	int qty;
	int *values;
} int_list_t;

extern const int config_form_qty;
extern const char config_form_titles[][64];

extern const int base_config_table_qty;
extern const int extended_config_table_qty;


int configuration_get_index(const char *name);
int configuration_get_info(unsigned int index, const config_info_t **info_ptr);
int configuration_read_value(unsigned int index, char *buffer, unsigned int buffer_len);
int configuration_write_value(unsigned int index, const char *buffer);
void configuration_load();
void configuration_cleanup();

