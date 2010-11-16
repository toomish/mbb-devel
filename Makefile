SUBDIRS = lib mbbd mbbsh

.PHONY: all $(SUBDIRS) clean distclean
all: glib.mk gthread.mk pgsql.mk
	@test -d bin || mkdir bin
	@test -d modules || mkdir modules
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

glib.mk: GLIB2_CFLAGS=$(shell pkg-config --cflags glib-2.0)
glib.mk:
	@echo GLIB2_CFLAGS = $(GLIB2_CFLAGS) >> $@
	@echo CFLAGS += $(GLIB2_CFLAGS) >> $@
	@echo LIBS += $(shell pkg-config --libs glib-2.0) >> $@

gthread.mk:
	@echo CFLAGS += $(shell pkg-config --cflags gthread-2.0) >> $@
	@echo LIBS += $(shell pkg-config --libs gthread-2.0) >> $@

pgsql.mk:
	@echo PGSQL_CFLAGS += -I$(shell pg_config --includedir) >> $@
	@echo LIBS += -L$(shell pg_config --libdir) -lpq >> $@

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

distclean: clean
	@rm -f mbbd/mbbd
	@rm -f mbbsh/mbbsh
	@rm -f glib.mk gthread.mk pgsql.mk
	@rm -rf modules
	@rm -rf bin
