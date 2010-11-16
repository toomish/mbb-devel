LIBS += -lft -lz

flow.o: flow.c
	$(CC) -c -o $@ $< -fPIC $(GLIB2_CFLAGS) -I$(ROOT)/include

