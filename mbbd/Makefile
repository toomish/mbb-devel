PROGNAME = $(shell pwd -P | xargs basename)
CLEANDEPS = modclean

ifndef $(ROOT)
	ROOT = $(shell pwd)/..
endif

export ROOT

sources = $(wildcard *.c)
objects = $(patsubst %.c,%.o,$(sources))
ext_objects = \
	../lib/datalink.o \
	../lib/debug.o \
	../lib/net.o \
	../lib/trash.o \
	../lib/variant.o \
	../lib/xmlparser.o \
	../lib/xmltag.o \
	../lib/shared.o \
	../lib/query.o \
	../lib/strconv.o \
	../lib/re.o \
	../lib/strerr.o

.PHONY: all modules
all: $(PROGNAME) modules
	@cp -v $(PROGNAME) ../bin

modules:
	@$(MAKE) -C modules

modclean:
	@$(MAKE) -C modules clean

$(PROGNAME): $(objects) $(ext_objects)
	$(CC) -rdynamic -o $@ $^ $(LIBS)

psql.o: psql.c
	$(CC) $(CFLAGS) $(PGSQL_CFLAGS) -o $@ $<

signaller.o: signaller.c
	$(CC) $(CFLAGS) -D_GNU_SOURCE -o $@ $<

include $(ROOT)/common.mk
include $(ROOT)/glib.mk
include $(ROOT)/gthread.mk
include $(ROOT)/pgsql.mk

CFLAGS += -I../include
LIBS += -ldl

