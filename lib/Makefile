sources = $(wildcard *.c)
objects = $(patsubst %.c,%.o,$(sources))

.PHONY: all
all: $(objects)

include ../common.mk
include ../glib.mk

CFLAGS += -I../include

