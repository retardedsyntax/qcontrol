VERSION=0.5.6

CFLAGS   += -c -g -Os -Wall -Wextra
CPPFLAGS += -DQCONTROL_VERSION=\"$(VERSION)\"

PKG_CONFIG ?= pkg-config
LDFLAGS  += -g

CFLAGS       += $(shell $(PKG_CONFIG) --cflags lua5.1)
LIBS_DYNAMIC += -lpthread $(shell $(PKG_CONFIG) --libs lua5.1)
LIBS_STATIC  += -lpthread $(shell $(PKG_CONFIG) --variable=libdir lua5.1)/liblua5.1.a -lm -ldl

ifeq ($(shell $(PKG_CONFIG) --exists libsystemd-daemon 2>/dev/null && echo 1),1)
CPPFLAGS_DYNAMIC += -DHAVE_SYSTEMD
CFLAGS_DYNAMIC   += $(shell $(PKG_CONFIG) --cflags libsystemd-daemon)
LIBS_DYNAMIC     += $(shell $(PKG_CONFIG) --libs libsystemd-daemon)
endif

SOURCES=qcontrol.c system.c qnap-pic.c ts209.c ts219.c ts409.c ts41x.c evdev.c a125.c synology.c
OBJECTS_DYNAMIC=$(SOURCES:.c=.o-dyn)
OBJECTS_STATIC=$(SOURCES:.c=.o-static)
EXECUTABLE=qcontrol

all:	$(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS_DYNAMIC)
	$(CC) $(LDFLAGS) $(OBJECTS_DYNAMIC) $(LIBS_DYNAMIC) -o $@

$(EXECUTABLE)-static: $(OBJECTS_STATIC)
	$(CC) $(LDFLAGS) $(OBJECTS_STATIC) $(LIBS_STATIC) -o $@

$(OBJECTS_DYNAMIC): CPPFLAGS += $(CPPFLAGS_DYNAMIC)
$(OBJECTS_DYNAMIC): CFLAGS += $(CFLAGS_DYNAMIC)
%.o-dyn: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

$(OBJECTS_STATIC): CPPFLAGS += $(CPPFLAGS_STATIC)
$(OBJECTS_STATIC): CFLAGS += $(CFLAGS_STATIC)
%.o-static: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS_DYNAMIC) $(OBJECTS_STATIC) $(EXECUTABLE) $(EXECUTABLE)-static

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
