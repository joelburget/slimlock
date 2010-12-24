# slimlock makefile
# © 2010 Joel Burget

CXX = g++
CC  = gcc

CFLAGS=-Wall -I. -I/usr/include/freetype2 -I/usr/include/freetype2/config -I/usr/include/libpng12 -I/usr/include
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lXft -lX11 -lfreetype -lXrender -lfontconfig -lpng14 -lz -lm -lcrypt -lXmu -lpng -ljpeg -lrt
CUSTOM=-DHAVE_SHADOW
NAME=slimlock
VERSION=0.1
CFGDIR=/etc# Change me!
DESTDIR=
PREFIX=/usr
DEFINES=-DPACKAGE=\"$(NAME)\" -DVERSION=\"$(VERSION)\" \
		-DPKGDATADIR=\"$(PREFIX)/share/slim\" -DSYSCONFDIR=\"$(CFGDIR)\"

OBJECTS=cfg.o image.o panel.o switchuser.o slimlock.o util.o jpeg.o png.o

all: slimlock

slimlock: $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $(NAME)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(DEFINES) $(CUSTOM) -c $< -o $@

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(CUSTOM) -c $< -o $@

clean:
	@rm -f slimlock *.o

install: slimlock
	install -D -m 755 slimlock $(DESTDIR)$(PREFIX)/bin/slimlock
