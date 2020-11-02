#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <FreeRTOS.h>

#include "arch/cc.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include <espressif/spi_flash.h>
#include <espressif/esp_system.h>
#include <espressif/esp_common.h>

#include "bearssl.h"
#include "http_ota.h"
#include "rboot-api.h"
#include "rboot.h"
#include "http_buffered_client.h"

#define MAX_IMAGE_SIZE        0x100000 /* 1MB is the maximum supported image size for OTA */
#define READ_BUFFER_LEN       512

#if SECTOR_SIZE % READ_BUFFER_LEN != 0
# error "Incompatible READ_BUFFER_LEN"
#endif

static uint32_t flash_offset;
static uint32_t flash_limits;

const char* ota_err_str(int code) {
	switch(code) {
		case OTA_UPDATE_DONE:
			return (const char*) &("OTA_UPDATE_DONE");
		case OTA_HASH_DONT_MATCH:
			return (const char*) &("OTA_HASH_DONT_MATCH");
		case OTA_ONE_SLOT_ONLY:
			return (const char*) &("OTA_ONE_SLOT_ONLY");
		case OTA_FAIL_SET_NEW_SLOT:
			return (const char*) &("OTA_FAIL_SET_NEW_SLOT");
		case OTA_IMAGE_VERIFY_FAILED:
			return (const char*) &("OTA_IMAGE_VERIFY_FAILED");
		case HTTP_DNS_LOOKUP_FAILED:
			return (const char*) &("HTTP_DNS_LOOKUP_FAILED");
		case HTTP_SOCKET_CREATION_FAILED:
			return (const char*) &("HTTP_SOCKET_CREATION_FAILED");
		case HTTP_CONNECTION_FAILED:
			return (const char*) &("HTTP_CONNECTION_FAILED");
		case HTTP_REQUEST_SEND_FAILED:
			return (const char*) &("HTTP_REQUEST_SEND_FAILED");
		case HTTP_DOWLOAD_SIZE_DONT_MATCH:
			return (const char*) &("HTTP_DOWLOAD_SIZE_DONT_MATCH");
		case HTTP_NOTFOUND:
			return (const char*) &("HTTP_NOTFOUND");
		default:
			return (const char*) &("UNKNOWN_ERROR");
	}
	
	return NULL;
}

static unsigned int ota_download_callback(char *buf, uint16_t size) {
	if (flash_offset + size > flash_limits) {
		return -1;
	}
	
	if (flash_offset % SECTOR_SIZE == 0) {
		unsigned int sector;
		
		sector = flash_offset / SECTOR_SIZE;
		sdk_spi_flash_erase_sector(sector);
	}
	
	sdk_spi_flash_write(flash_offset, (uint32_t *) buf, size);
	flash_offset += size;
	return 1;
}

OTA_err ota_update(OTA_config_t *ota_config) {
	HTTP_client_info http_inf;
	rboot_config rboot_config;
	HTTP_err err;
	uint8_t new_rom_slot;
	uint32_t image_length;
	
	http_inf.buffer = malloc(SECTOR_SIZE + 4);
	http_inf.buffer_size = SECTOR_SIZE;
	http_inf.server = ota_config->server;
	http_inf.port = ota_config->port;
	
	/* sdk_spi_flash_write requires a word (32 bits) aligned buffer */
	while((uint32_t)http_inf.buffer % 4) {
		http_inf.buffer++;
	}
	
	rboot_config = rboot_get_config();
	new_rom_slot = (rboot_config.current_rom + 1) % rboot_config.count;
	
	if (new_rom_slot == rboot_config.current_rom) {
		free(http_inf.buffer);
		return OTA_ONE_SLOT_ONLY;
	}
	
	flash_offset = rboot_config.roms[new_rom_slot];
	flash_limits = flash_offset + MAX_IMAGE_SIZE;
	
	http_inf.path           = ota_config->path;
	http_inf.final_cb       = ota_download_callback;
	http_inf.buffer_full_cb = ota_download_callback;

	err = HttpClient_dowload(&http_inf);
	
	free(http_inf.buffer);
	
	if (err != HTTP_OK)
		return err;
	
	vTaskDelay(pdMS_TO_TICKS(500)); /* avoid wdt reset */
	
	if (!rboot_verify_image(rboot_config.roms[new_rom_slot], &image_length, NULL))
		return OTA_IMAGE_VERIFY_FAILED;
	
	vTaskDelay(pdMS_TO_TICKS(300)); /* avoid wdt reset */
	
	if (ota_config->hash_str != NULL) {
		br_sha256_context *hash_ctx;
		uint8_t *hash_output;
		char *hash_str;
		
		hash_ctx = (br_sha256_context*) malloc(sizeof(br_sha256_context));
		hash_output = (uint8_t*) malloc(br_sha256_SIZE);
		hash_str = (char*) malloc(br_sha256_SIZE * 2 + 1);
		
		br_sha256_init(hash_ctx);
		
		rboot_digest_image(rboot_config.roms[new_rom_slot], image_length, (rboot_digest_update_fn)br_sha256_update, (void*)hash_ctx);
		
		vTaskDelay(pdMS_TO_TICKS(300)); /* avoid wdt reset */
		
		br_sha256_out(hash_ctx, hash_output);
		
		free(hash_ctx);
		
		for(int i = 0; i < br_sha256_SIZE; i++)
			sprintf(hash_str + i * 2, "%02hx", hash_output[i]);
		
		free(hash_output);
		
		if (strcmp(hash_str, ota_config->hash_str)) {
			free(hash_str);
			return OTA_HASH_DONT_MATCH;
		}
		
		free(hash_str);
	}
	
	rboot_config.current_rom = new_rom_slot;
	
	vTaskDelay(pdMS_TO_TICKS(300)); /* avoid wdt reset */
	
	vPortEnterCritical();
	if (!rboot_set_config(&rboot_config)) {
		vPortExitCritical();
		return OTA_FAIL_SET_NEW_SLOT;
	}
	vPortExitCritical();
	
	return OTA_UPDATE_DONE;
}
