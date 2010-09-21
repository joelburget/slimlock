# Customize below to fit your system

# paths
PREFIX = /usr/local

X11INC = /usr/X11R6/include

# includes and libs
INCS = -I. -I/usr/include -I${X11INC} 
LIBS = -L/usr/lib -lc -lcrypt -lX11 -lXext -lXft -ljpeg -lpng -lpng12 -lfreetype -lXrender -lfontconfig 

# flags
CFLAGS=-Wall -I. -I/usr/include/freetype2 -I/usr/include/freetype2/config -I/usr/include/libpng12 -I/usr/include
CXXFLAGS=$(CFLAGS)
LDFLAGS = -s ${LIBS}

NAME=slimlock
VERSION=0.1
CFGDIR=/etc

OBJECTS=cfg.o image.o panel.o switchuser.o slimlock.o util.o jpeg.o png.o

DEFINES=-DPACKAGE=\"$(NAME)\" -DVERSION=\"$(VERSION)\" \
		-DPKGDATADIR=\"$(PREFIX)/share/slim\" -DSYSCONFDIR=\"$(CFGDIR)\"

# On *BSD remove -DHAVE_SHADOW_H from CXXFLAGS and add -DHAVE_BSD_AUTH
# On OpenBSD and Darwin remove -lcrypt from LIBS

# compiler and linker
CXX = g++
CC = gcc

# Install mode. On BSD systems MODE=2755 and GROUP=auth
# On others MODE=4755 and GROUP=root
#MODE=2755
#GROUP=auth
