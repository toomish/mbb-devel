MODULES = netflow attr auth_apache # var pinger
MODMK = ../module.mk

export ROOT

.PHONY: all
all:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir -f $(MODMK); \
	done

clean:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir -f $(MODMK) clean; \
	done

