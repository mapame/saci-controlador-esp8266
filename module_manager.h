int update_module_list();
int fetch_values(int update_all);
int module_get_info(unsigned int module_addr, char *name_buffer, unsigned int buffer_len);
int module_get_channel_info(unsigned int module_addr, unsigned int channeln, char *name_buffer, char *type, char *access);
int module_set_port_enable_update(unsigned int module_addr, unsigned int channeln, unsigned int portn);
int module_get_channel_bounds(unsigned int module_addr, unsigned int channeln, void *min, void *max);
int set_port_value(unsigned int module_addr, unsigned int channeln, unsigned int portn, const void *value);
int get_port_value(unsigned int module_addr, unsigned int channeln, unsigned int portn, void *value_buffer);
int get_port_value_text(unsigned int module_addr, unsigned int channeln, unsigned int portn, char *buffer, unsigned int buffer_len);
