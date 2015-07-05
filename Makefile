SBIN_RFIDEXEC  = rfidexec

CC ?= gcc
CFLAGS ?= -Wall -g -O2 -pipe #-DDEBUG
INCLUDES ?= -Ic_hashmap
PREFIX ?= /usr/local
INSTALL ?= install
STRIP ?= strip
BINDIR  ?= $(DESTDIR)$(PREFIX)/bin
SHAREDIR ?= $(DESTDIR)$(PREFIX)/share

all: $(SBIN_RFIDEXEC)

rfidexec.o: rfidexec.c debug.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

mapping.o: mapping.c mapping.h debug.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

hashmap.o: c_hashmap/hashmap.c c_hashmap/hashmap.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

rfidexec: rfidexec.o mapping.o c_hashmap/hashmap.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ rfidexec.o mapping.o c_hashmap/hashmap.o

install: install-sbin

install-sbin: $(SBIN_RFIDEXEC)
	mkdir -p $(BINDIR)
	$(STRIP) $(SBIN_RFIDEXEC)
	$(INSTALL) $(SBIN_RFIDEXEC) $(BINDIR)/

clean:
	rm -f $(SBIN_RFIDEXEC) *.o c_hashmap/hashmap.o
