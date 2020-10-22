#include <mqtt.h>
#include <bearssl.h>

int brssl_mqtt_init(bearssl_context *ctx, size_t ssl_buffer_size, const br_x509_trust_anchor *trust_anchors, size_t trust_anchors_num);
int brssl_mqtt_connect(bearssl_context *ctx, const char *host, const char *port, time_t timenow);
int brssl_mqtt_close(bearssl_context *ctx);
void brssl_mqtt_free(bearssl_context *ctx);
