TARGET		:= dualsensectl
VERSION		:= 0.6

OPTFLAGS	= -O2 -s -DNDEBUG
CFLAGS		+= $(OPTFLAGS)

CFLAGS		+= -DDUALSENSECTL_VERSION=\"$(VERSION)\"
CFLAGS		+= -Wall -Wextra -pedantic
CFLAGS		+= $(shell pkg-config --cflags dbus-1)
CFLAGS		+= $(shell pkg-config --cflags hidapi-hidraw)
LDFLAGS		+= $(shell pkg-config --libs dbus-1)
LDFLAGS		+= $(shell pkg-config --libs hidapi-hidraw)
LDFLAGS		+= $(shell pkg-config --libs libudev)

all: $(TARGET)

.PHONE: debug
debug: OPTFLAGS = -Og -g
debug: all

install: all
	install -D -m 755 -p $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)
	install -D -m 644 -p completion/$(TARGET) $(DESTDIR)/usr/share/bash-completion/completions/$(TARGET)
	install -D -m 644 -p completion/_$(TARGET) $(DESTDIR)/usr/share/zsh/site-functions/_$(TARGET)

uninstall:
	rm -f $(DESTDIR)/usr/bin/$(TARGET)
	rm -f $(DESTDIR)/usr/share/bash-completion/completions/$(TARGET)
	rm -f $(DESTDIR)/usr/share/zsh/site-functions/_$(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) $< -o $(TARGET) $(LDFLAGS)
