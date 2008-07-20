CFLAGS=-Os -Wall -I /usr/include/lua5.1
LDFLAGS=-llua5.1 -lpthread
SOURCES=qcontrol.c ts209.c ts409.c evdev.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=qcontrol

all:	$(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
	
