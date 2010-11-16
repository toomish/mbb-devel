PROGNAME = $(shell pwd -P | xargs basename)
LUAPKG = lua5.1

sources = $(wildcard *.c)
objects = $(patsubst %.c,%.o,$(sources))
ext_objects = \
	../lib/debug.o \
	../lib/net.o \
	../lib/trash.o \
	../lib/xmltag.o \
	../lib/variant.o \
	../lib/xmlparser.o \
	../lib/strconv.o

.PHONY: all
all: $(PROGNAME) lua.mk
	@cp -v $< ../bin

$(PROGNAME): $(objects) $(ext_objects)
	$(CC) -o $@ $^ $(LIBS)

include ../common.mk
include ../glib.mk
include ../gthread.mk

lua.mk:
ifeq ($(shell pkg-config --libs $(LUAPKG) 2>/dev/null),)
	$(error missing package $(LUAPKG))
endif
	@echo CFLAGS += $(shell pkg-config --cflags $(LUAPKG)) >> $@
	@echo LIBS += $(shell pkg-config --libs $(LUAPKG)) >> $@

sinclude lua.mk

CFLAGS += -I../include -D_BSD_SOURCE -DUSE_GNU_READLINE
LIBS += -lreadline -lcurses

