# build executable on typing make
TARGET = sdk

STRIP	= strip
CC = gcc 
OBJECTS = sdk.o parser_sdk.o log.o serial.o server.o globals.o linux.o mini_snmpd.o protocol.o utils.o mib.o
VERSION = 1.2b
VENDOR	= .1.3.6.1.4.1
OFLAGS	= -O2 
CFLAGS	= -Wall -DVERSION="\"$(VERSION)\"" -DVENDOR="\"$(VENDOR)\"" \
	  $(OFLAGS) -D__TRAPS__ -D__LINUX__ -D__IPV6__ -D__DEMO__ -D__SDK__
LDFLAGS	= $(OFLAGS)


all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -I. -Iinclude -o $@ $^

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ -L. -lpthread

	
strip: $(TARGET)
	$(STRIP) $(TARGET)
clean:
	rm -f *.o sdk
