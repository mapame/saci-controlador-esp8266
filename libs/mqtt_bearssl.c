#include "espressif/esp_common.h"
#include "esp/hwrand.h"

#include <unistd.h>
#include <time.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/api.h"

#include "mqtt.h"

#define CLOCK_SECONDS_PER_DAY (24*3600)


static int socket_read(void *ctx, unsigned char *buf, size_t len) {
	ssize_t rlen;
	for (;;) {
		rlen = recv(*(int *)ctx, buf, len, 0);
		
		if (rlen < 0) {
			if (errno == EINTR) {
				continue;
			} else if (errno == EWOULDBLOCK || errno == EAGAIN) {
				rlen = 0;
				break;
			} else {
				break;
			}
		} else {
			break;
		}
	}
	return (int)rlen;
}

static int socket_write(void *ctx, const unsigned char *buf, size_t len) {
	ssize_t wlen;
	
	for (;;) {
		wlen = write(*(int *)ctx, buf, len);
		
		if (wlen <= 0 && errno == EINTR) {
			continue;
		}
		return (int)wlen;
	}
	
	return 0;
}


static int socket_connect(const char *host, const char *port) {
	int sockfd = -1;
	
	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	
	struct addrinfo *res = NULL;
	
	if(getaddrinfo(host, port, &hints, &res) != 0 || res == NULL)
		return -1;
	
	if((sockfd = socket(res->ai_family, res->ai_socktype, 0)) == -1)
		return -1;
	
	if(connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
		close(sockfd);
		return -1;
	}
	
	lwip_fcntl(sockfd, F_SETFL, O_NONBLOCK);
	
	return sockfd;
}

int brssl_mqtt_init(bearssl_context *ctx, size_t ssl_buffer_size, const br_x509_trust_anchor *trust_anchors, size_t trust_anchors_num) {
	ctx->brssl_ibuffer = (unsigned char*) malloc((ssl_buffer_size + 325 + 1) * sizeof(unsigned char));
	ctx->brssl_obuffer = (unsigned char*) malloc((ssl_buffer_size + 85 + 1) * sizeof(unsigned char));
	
	if(ctx->brssl_ibuffer == NULL || ctx->brssl_ibuffer == NULL) {
		free(ctx->brssl_ibuffer);
		free(ctx->brssl_obuffer);
		
		return -1;
	}
	
	br_ssl_client_init_full(&ctx->cc, &ctx->xc, trust_anchors, trust_anchors_num);
	br_ssl_engine_set_buffers_bidi(&ctx->cc.eng, ctx->brssl_ibuffer, ((ssl_buffer_size + 325 + 1) * sizeof(unsigned char)), ctx->brssl_obuffer, ((ssl_buffer_size + 85 + 1) * sizeof(unsigned char)));
	
	for (int i = 0; i < 10; i++) {
		int rand = hwrand();
		br_ssl_engine_inject_entropy(&ctx->cc.eng, &rand, 4);
	}
	
	ctx->low_read = socket_read;
	ctx->low_write = socket_write;
	
	return 0;
}

int brssl_mqtt_connect(bearssl_context *ctx, const char *host, const char *port, time_t timenow) {
	if(br_ssl_client_reset(&ctx->cc, host, 0) == 0)
		return -1;
	
	br_x509_minimal_set_time(&ctx->xc, ((timenow / CLOCK_SECONDS_PER_DAY) + 719528), (timenow % CLOCK_SECONDS_PER_DAY));
	
	if ((ctx->fd = socket_connect(host, port)) == -1)
		return -2;
	
	return 0;
}

int brssl_mqtt_close(bearssl_context *ctx) {
	int rc = 0;
	
	br_ssl_engine_close(&ctx->cc.eng);
	
	if (ctx->fd != 0) {
		shutdown(ctx->fd, SHUT_RDWR);
		rc = close(ctx->fd);
		ctx->fd = 0;
	}
	
	return rc;
}

void brssl_mqtt_free(bearssl_context *ctx) {
	free(ctx->brssl_ibuffer);
	free(ctx->brssl_obuffer);
}
