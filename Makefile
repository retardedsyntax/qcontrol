VERSION=0.5.6

CFLAGS   += -c -g -Os -Wall -Wextra
CPPFLAGS += -DQCONTROL_VERSION=\"$(VERSION)\"

PKG_CONFIG ?= pkg-config
LDFLAGS  += -g
LIBS     += -lpthread
LIBS_STATIC += /usr/lib/liblua5.1.a -lpthread -lm -ldl

CFLAGS   += $(shell $(PKG_CONFIG) --cflags lua5.1)
LIBS     += $(shell $(PKG_CONFIG) --libs lua5.1)

ifeq ($(shell $(PKG_CONFIG) --exists libsystemd-daemon 2>/dev/null && echo 1),1)
CPPFLAGS += -DHAVE_SYSTEMD
CFLAGS   += $(shell $(PKG_CONFIG) --cflags libsystemd-daemon)
LIBS     += $(shell $(PKG_CONFIG) --libs libsystemd-daemon)
endif

SOURCES=qcontrol.c system.c qnap-pic.c ts209.c ts219.c ts409.c ts41x.c evdev.c a125.c synology.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=qcontrol

all:	$(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

$(EXECUTABLE)-static: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS_STATIC) -o $@

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) $(EXECUTABLE)-static

dist: RELEASES := $(PWD)/../releases/
dist: TARBALL := qcontrol-$(VERSION).tar
dist: TAG := v$(VERSION)
dist:
	git tag -v $(TAG)
	git archive --format=tar --prefix=qcontrol-$(VERSION)/ --output $(RELEASES)/$(TARBALL) $(TAG)
	cat $(RELEASES)/$(TARBALL) | gzip -c9 > $(RELEASES)/$(TARBALL).gz
	xz --stdout --compress $(RELEASES)/$(TARBALL) > $(RELEASES)/$(TARBALL).xz
	cd $(RELEASES) && sha256sum $(TARBALL) $(TARBALL).gz $(TARBALL).xz > $(TARBALL).sha256

$(OBJECTS): picmodule.h
