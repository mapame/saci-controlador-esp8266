int mqtt_task_publish_text(const char* topic, const char* text, int qos, int retain);
int mqtt_task_publish_int(const char* topic, int value, int qos, int retain);
int mqtt_task_publish_float(const char* topic, float value, int qos, int retain);
