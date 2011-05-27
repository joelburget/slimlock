# slimlock makefile
# © 2010 Joel Burget

CXX = g++
CC  = gcc

CFLAGS=-Wall -I. -I/usr/include/freetype2 -I/usr/include/freetype2/config -I/usr/include/libpng14 -I/usr/include
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lXft -lX11 -lfontconfig -lpng14 -lz -lm -lcrypt -lXmu -lpng -ljpeg -lrt
CUSTOM=-DHAVE_SHADOW
NAME=slimlock
VERSION=0.8
CFGDIR=/etc
MANDIR=/usr/man
DESTDIR=
PREFIX=/usr
DEFINES=-DPACKAGE=\"$(NAME)\" -DVERSION=\"$(VERSION)\" \
		-DPKGDATADIR=\"$(PREFIX)/share/slim\" -DSYSCONFDIR=\"$(CFGDIR)\"

OBJECTS=cfg.o image.o panel.o slimlock.o util.o jpeg.o png.o

all: slimlock
	@chmod 4755 slimlock

slimlock: $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $(NAME)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(DEFINES) $(CUSTOM) -c $< -o $@

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(CUSTOM) -c $< -o $@

clean:
	@rm -f slimlock *.o

dist:
	@rm -rf $(NAME)-$(VERSION)
	@mkdir $(NAME)-$(VERSION)
	@cp -r *.cpp *.h *.c Makefile LICENSE README.md slimlock.1 slimlock.conf $(NAME)-$(VERSION)
	@tar cvzf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION)
	@rm -rf $(NAME)-$(VERSION)

install: slimlock
	@install -D -m 644 slimlock.1 $(DESTDIR)$(MANDIR)/man1/slimlock.1
	@install -D -m 4755 slimlock $(DESTDIR)$(PREFIX)/bin/slimlock
	@install -D -m 644 slimlock.conf $(DESTDIR)$(CFGDIR)/slimlock.conf
