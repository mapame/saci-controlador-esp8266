PROGRAM = saci_firmware

FLASH_SIZE = 32

SPIFFS_BASE_ADDR = 0x302000
SPIFFS_SIZE = 0xf2000

PROGRAM_SRC_DIR=. ./libs ./custom_code
PROGRAM_INC_DIR=. ./libs
EXTRA_CFLAGS=-I./fsdata

EXTRA_COMPONENTS = extras/dhcpserver extras/rboot-ota extras/bearssl extras/mbedtls extras/httpd extras/ds3231 extras/i2c
#ESPBAUD = 460800
LIBS ?= gcc hal m
include /mnt/HDD0A/Software/ESP8266/esp-open-rtos/common.mk

html:
	@echo "Generating fsdata.."
	cd fsdata && ./makefsdata
