CC = gcc
CFLAGS += -Wall -Wextra -pedantic
CFLAGS += $(shell pkg-config --cflags dbus-1)
CFLAGS += $(shell pkg-config --cflags hidapi-hidraw)
LIBS   += $(shell pkg-config --libs dbus-1)
LIBS   += $(shell pkg-config --libs hidapi-hidraw)

TARGET = dualsensectl
VERSION = 0.2

ifeq ($(BUILD),debug)
CFLAGS += -O0 -g
else
CFLAGS += -O2 -s -DNDEBUG
endif

DEFINES += -DDUALSENSECTL_VERSION=\"$(VERSION)\"

all:
	$(CC) main.c -o $(TARGET) $(DEFINES) $(CFLAGS) $(LIBS)

debug:
	make "BUILD=debug"

install: all
	install -D -m 755 -p $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)
	install -D -m 644 -p completion/$(TARGET) $(DESTDIR)/usr/share/bash-completion/completions/$(TARGET)
	install -D -m 644 -p completion/_$(TARGET) $(DESTDIR)/usr/share/zsh/site-functions/_$(TARGET)

uninstall:
	rm -f $(DESTDIR)/usr/bin/$(TARGET)
	rm -f $(DESTDIR)/usr/share/bash-completion/completions/$(TARGET)
	rm -f $(DESTDIR)/usr/share/zsh/site-functions/_$(TARGET)
