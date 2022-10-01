ifeq ($(PREFIX), )
	PREFIX = /usr
endif

ifeq ($(MANPREFIX), )
	MANPREFIX = $(PREFIX)/share/man
endif

PHONY = all clean install uninstall

all:

install:
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	install -oroot -groot -m 644 man/mwm.1 $(DESTDIR)$(MANPREFIX)/man1/mwm.1

uninstall:
	rm -f $(DESTDIR)$(MANPREFIX)/man1/mwm.1

clean:

.PHONY: $(PHONY)
