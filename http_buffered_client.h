#ifndef HTTP_BUFFERED_CLIENT_H
#define HTTP_BUFFERED_CLIENT_H

typedef unsigned int (*http_cb)(char *buff, uint16_t size);

typedef enum  {
	HTTP_OK = 200,
	HTTP_NOTFOUND = 404,
	HTTP_DNS_LOOKUP_FAILED = 1000,
	HTTP_SOCKET_CREATION_FAILED,
	HTTP_CONNECTION_FAILED,
	HTTP_REQUEST_SEND_FAILED,
	HTTP_DOWLOAD_SIZE_DONT_MATCH,
} HTTP_err;

typedef struct  {
	const char *  server;
	const char *  port;
	const char *  path;
	char *        buffer;
	uint16_t      buffer_size;
	http_cb buffer_full_cb;
	http_cb final_cb;
} HTTP_client_info;

HTTP_err HttpClient_dowload(HTTP_client_info *info);

#endif // ifndef HTTP_BUFFERED_CLIENT_H
