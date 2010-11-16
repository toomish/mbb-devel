apr-util.mk:
	@echo CFLAGS += $(shell pkg-config --cflags apr-util-1) >> $@
	@echo LIBS += $(shell pkg-config --libs apr-util-1) >> $@

sinclude apr-util.mk

CFLAGS += -I..

