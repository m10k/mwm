OUTPUT = mwm
OBJECTS = main.o x.o monitor.o mwm.o workspace.o loop.o client.o common.o layout.o config.o

ifeq ($(PREFIX), )
	PREFIX = /usr/local
endif
ifeq ($(MANPREFIX), )
	MANPREFIX = $(PREFIX)/share/man
endif

PKG_CONFIG_LIBS = xft pango pangoxft
CFLAGS = -std=c99 -pedantic -Wall $(shell pkg-config --cflags $(PKG_CONFIG_LIBS))
LDFLAGS = -lX11 -lXinerama $(shell pkg-config --libs $(PKG_CONFIG_LIBS))

PHONY = all clean install uninstall

ifeq ($(DEBUG), 1)
	CFLAGS += -g
	LDFLAGS += -g
endif

all: mwm

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -oroot -groot -m755 mwm $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	install -oroot -groot -m 644 docs/man/mwm.1 $(DESTDIR)$(MANPREFIX)/man1/mwm.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mwm
	rm -f $(DESTDIR)$(MANPREFIX)/man1/mwm.1

mwm: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJECTS)

.PHONY: $(PHONY)
