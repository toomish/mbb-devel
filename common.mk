CFLAGS = -c -g -Wall -Wextra -Werror -pedantic -std=c99 -D_POSIX_SOURCE -DNDEBUG -DNOCOLOR

strcat = $(patsubst %, $(1)%, $(2))
prefix = $(foreach str, $(2), $(call strcat, $(1), $(str)))

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean: $(CLEANDEPS)
	@echo cleaning...
	@rm -f *.o *~ core.* *.so
