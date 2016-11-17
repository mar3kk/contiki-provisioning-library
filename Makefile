PACKAGES_DIR=../../packages
CONTIKI_PROJECT=example
VERSION?=$(shell git describe --abbrev=4 --dirty --always --tags)
CONTIKI=../../contiki
CONTIKI_WITH_IPV6=1
CONTIKI_WITH_RPL=0
USE_SERIAL_PADS=1

TARGET=mikro-e

CFLAGS += -DWITH_AES_DECRYPT=1

#6lowpan type
#USE_CA8210=1
USE_CC2520=1

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\" -DVERSION='\"$(VERSION)\"'
CFLAGS += -Wall -Wno-pointer-sign -O3

#needed by tinyDTLS
CFLAGS += -DDTLS_PEERS_NOHASH=1

PROJECT_SOURCEFILES += provision_communication.c provision_config.c provision_crypto.c provision_library.c bigint.c diffie_hellman_keys_exchanger.c

ifneq (,$(filter  $(TARGET),seedeye mikro-e))
  CFLAGS += -fno-short-double
  LDFLAGS += -Wl,--defsym,_min_heap_size=64000
endif

APPS += client
APPS += common
APPS += hmac
APPS += tinydtls/aes tinydtls/sha2 tinydtls/ecc tinydtls

ifeq ($(TARGET),minimal-net)
  APPS += xml
endif

APPDIRS += $(CONTIKI)/platform/mikro-e/dev
APPDIRS += $(PACKAGES_DIR)

all: $(CONTIKI_PROJECT)
	xc32-bin2hex $(CONTIKI_PROJECT).$(TARGET)

distclean: cleanall

cleanall:
	rm -f $(CONTIKI_PROJECT).hex
	rm -f symbols.*

include $(CONTIKI)/Makefile.include
