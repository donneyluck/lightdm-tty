PREFIX ?= /usr/local
THEME_DIR ?= /usr/share/lightdm-tty-theme

CFLAGS  := -Wall -Wextra -O2 $(shell pkg-config --cflags liblightdm-gobject-1 webkit2gtk-4.1 gtk+-3.0 json-glib-1.0)
LDFLAGS := $(shell pkg-config --libs   liblightdm-gobject-1 webkit2gtk-4.1 gtk+-3.0 json-glib-1.0)

all: lightdm-tty-greeter

lightdm-tty-greeter: src/greeter.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: lightdm-tty-greeter
	install -d $(DESTDIR)$(PREFIX)/lib/lightdm
	install -m 755 lightdm-tty-greeter $(DESTDIR)$(PREFIX)/lib/lightdm/
	install -d $(DESTDIR)$(THEME_DIR)
	cp -r index.html css js img $(DESTDIR)$(THEME_DIR)/
	@echo ""
	@echo "Installed. Now configure LightDM:"
	@echo "  sudo sh -c 'echo \"greeter-session=lightdm-tty-greeter\" >> /etc/lightdm/lightdm.conf'"

clean:
	rm -f lightdm-tty-greeter

.PHONY: all install clean
