MODULE = $(shell pwd -P | xargs basename).so

sources = $(wildcard *.c)
objects = $(patsubst %.c,%.o,$(sources))

.PHONY: all
all: $(MODULE)
	install -m 555 $< $(ROOT)/modules

$(MODULE): $(objects)
	$(CC) -o $@ $^ $(LIBS)

include $(ROOT)/common.mk
include $(ROOT)/glib.mk
-include include.mk

CFLAGS += -fPIC -I$(ROOT)/include -I$(ROOT)/mbbd
LIBS += -shared

ifndef ROOT
	$(error ROOT not defined)
endif

