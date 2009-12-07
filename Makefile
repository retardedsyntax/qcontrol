CFLAGS=-Os -Wall
CPPFLAGS=-I /usr/include/lua5.1
LDFLAGS=
LIBS=-llua5.1 -lpthread
SOURCES=qcontrol.c ts209.c ts219.c ts409.c ts41x.c a125.c evdev.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=qcontrol

all:	$(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

.cpp.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
