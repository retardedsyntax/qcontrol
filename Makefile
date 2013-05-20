VERSION=0.5.2~pre

CFLAGS   += -Os -Wall -DQCONTROL_VERSION=\"$(VERSION)\"
CPPFLAGS += -I/usr/include/lua5.1
LDFLAGS  +=
LIBS     += -llua5.1 -lpthread
LIBS_STATIC += /usr/lib/liblua5.1.a -lpthread -lm -ldl

SOURCES=qcontrol.c ts209.c ts219.c ts409.c ts41x.c evdev.c a125.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=qcontrol

all:	$(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

$(EXECUTABLE)-static: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LIBS_STATIC) -o $@

.cpp.o:
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
