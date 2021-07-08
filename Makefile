CC = gcc
CFLAGS += -Wall -Wextra -pedantic
CFLAGS += $(shell pkg-config --cflags dbus-1)
CFLAGS += $(shell pkg-config --cflags hidapi-hidraw)
LIBS   += $(shell pkg-config --libs dbus-1)
LIBS   += $(shell pkg-config --libs hidapi-hidraw)

TARGET = dualsensectl

ifeq ($(BUILD),debug)
CFLAGS += -O0 -g
else
CFLAGS += -O2 -s -DNDEBUG
endif

all:
	$(CC) -o $(TARGET) $(CFLAGS) $(LIBS) main.c

debug:
	make "BUILD=debug"

install: all
	install -m 755 -p $(TARGET) /usr/bin/$(TARGET)

uninstall:
	rm -f /usr/bin/$(TARGET)
