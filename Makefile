PROGRAM = master-test

FLASH_SIZE = 32

EXTRA_COMPONENTS = extras/dhcpserver extras/rboot-ota extras/bearssl extras/ds3231 extras/i2c
#ESPBAUD = 460800
LIBS ?= gcc hal m
include /mnt/HDD0A/Software/ESP8266/esp-open-rtos/common.mk
