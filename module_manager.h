int module_get_info(unsigned int module_addr, char *name_buffer, unsigned int buffer_len);
int module_get_channel_info(unsigned int module_addr, unsigned int channeln, char *name_buffer, char *type, char *writable);
int module_get_channel_bounds(unsigned int module_addr, unsigned int channeln, float *min, float *max);

int module_get_port_value(unsigned int module_addr, unsigned int channeln, unsigned int portn, void *value_buffer);
int module_get_port_value_as_text(unsigned int module_addr, unsigned int channeln, unsigned int portn, char *buffer, unsigned int buffer_len);

int module_set_port_value(unsigned int module_addr, unsigned int channeln, unsigned int portn, const void *value);
int module_set_port_value_as_text(unsigned int module_addr, unsigned int channeln, unsigned int portn, const char *buffer);

int module_set_port_update_type(unsigned int module_addr, unsigned int channeln, unsigned int portn, char update_type);
