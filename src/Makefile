OUTPUT = mwm
OBJECTS = main.o x.o monitor.o mwm.o workspace.o loop.o client.o common.o layout.o \
          config.o kbptr.o xrandr.o

ifeq ($(PREFIX), )
	PREFIX = /usr
endif
ifeq ($(MANPREFIX), )
	MANPREFIX = $(PREFIX)/share/man
endif

PKG_CONFIG_LIBS = xft pango pangoxft
CFLAGS = -std=c99 -pedantic -Wall $(shell pkg-config --cflags $(PKG_CONFIG_LIBS))
LDFLAGS = -lX11 -lXrandr $(shell pkg-config --libs $(PKG_CONFIG_LIBS))

PHONY = all clean install uninstall

ifeq ($(DEBUG), 1)
	CFLAGS += -g
	LDFLAGS += -g
endif

all: mwm

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -oroot -groot -m755 mwm $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mwm

mwm: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJECTS)

.PHONY: $(PHONY)
