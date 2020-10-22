/*
MIT License

Copyright(c) 2018 Liam Bindle

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <mqtt.h>

/** 
 * @file 
 * @brief Implements @ref mqtt_pal_sendall and @ref mqtt_pal_recvall and 
 *        any platform-specific helpers you'd like.
 * @cond Doxygen_Suppress
 */

#include <bearssl.h>

static int do_rec_data(mqtt_pal_socket_handle fd, unsigned int status) {
	ssize_t rc;
	uint8_t *buffer;
	size_t length;
	int err;

	err = br_ssl_engine_last_error(&fd->cc.eng);

	if (err != BR_ERR_OK) {
		return MQTT_ERROR_SOCKET_ERROR;
	}

	if ((status & BR_SSL_SENDREC) == BR_SSL_SENDREC) {
		buffer = br_ssl_engine_sendrec_buf(&fd->cc.eng, &length);

		if (length > 0) {
			if ((rc = fd->low_write(&fd->fd, buffer, length)) < 0) {
				return MQTT_ERROR_SOCKET_ERROR;
			}

			br_ssl_engine_sendrec_ack(&fd->cc.eng, rc);
		}
	}
	else if ((status & BR_SSL_RECVREC) == BR_SSL_RECVREC) {
		buffer = br_ssl_engine_recvrec_buf(&fd->cc.eng, &length);

		if (length > 0) {
			if ((rc = fd->low_read(&fd->fd, buffer, length)) < 0) {
				return MQTT_ERROR_SOCKET_ERROR;
			}
			
			br_ssl_engine_recvrec_ack(&fd->cc.eng, rc);
		}
	}
	else if ((status && BR_SSL_CLOSED) == BR_SSL_CLOSED) {
		return MQTT_ERROR_SOCKET_ERROR;
	}

	return MQTT_OK;
}

ssize_t mqtt_pal_sendall(mqtt_pal_socket_handle fd, const void* buf, size_t len, int flags) {
	int rc = MQTT_OK;
	uint8_t *buffer;
	size_t length;
	size_t remaining_bytes = len;
	const uint8_t *walker = buf;
	unsigned int status;

	while (remaining_bytes > 0) {

		if (rc == MQTT_ERROR_SOCKET_ERROR) {
			return rc;
		}

		status = br_ssl_engine_current_state(&fd->cc.eng);

		if ((status & BR_SSL_CLOSED) != 0) {
			return MQTT_ERROR_SOCKET_ERROR;
		}

		if ((status & (BR_SSL_RECVREC | BR_SSL_SENDREC)) != 0) {
			rc = do_rec_data(fd, status);

			if (rc != MQTT_OK) {
				return rc;
			}
			status = br_ssl_engine_current_state(&fd->cc.eng);
		}

		if ((status & BR_SSL_SENDAPP) == BR_SSL_SENDAPP) {
			buffer = br_ssl_engine_sendapp_buf(&fd->cc.eng, &length);

			if (length > 0) {
				size_t write = (length >= remaining_bytes) ? remaining_bytes : length;
				memcpy(buffer, walker, write);
				remaining_bytes -= write;
				walker += write;
				br_ssl_engine_sendapp_ack(&fd->cc.eng, write);
				br_ssl_engine_flush(&fd->cc.eng, 0);
			}
		}
	}

	return len;
}

ssize_t mqtt_pal_recvall(mqtt_pal_socket_handle fd, void* buf, size_t bufsz, int flags) {
	int rc;
	uint8_t *buffer;
	size_t length;
	unsigned int status;
	size_t write = 0;

	status = br_ssl_engine_current_state(&fd->cc.eng);

	if ((status & (BR_SSL_RECVREC | BR_SSL_SENDREC)) != 0) {
		rc = do_rec_data(fd, status);
		
		if (rc != MQTT_OK) {
			return rc;
		}
		status = br_ssl_engine_current_state(&fd->cc.eng);
	}
	
	if ((status & BR_SSL_RECVAPP) == BR_SSL_RECVAPP) {
		buffer = br_ssl_engine_recvapp_buf(&fd->cc.eng, &length);
		
		if (length > 0) {
			write = (length >= bufsz) ? bufsz : length;
			memcpy(buf, buffer, write);
			br_ssl_engine_recvapp_ack(&fd->cc.eng, write);
		}
	}
	
	return write;
}

/** @endcond */
