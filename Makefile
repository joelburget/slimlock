# slock - simple screen locker
# © 2006-2007 Anselm R. Garbe, Sander van Dijk

include config.mk

SRC = slimlock.cpp
OBJ = ${SRC:.c=.o}

all: options slimlock

options:
	@echo slimlock build options:
	@echo "CXXFLAGS  = ${CXXFLAGS}"
	@echo "LDFLAGS   = ${LDFLAGS}"
	@echo "CXX       = ${CXX}"
	@echo "CC        = ${CC}"
	@echo "OBJ       = ${OBJ}"
	@echo "OBJECTS   = ${OBJECTS}"

#.c.o:
#	@echo CXX $<
#	@${CC} -c ${DEFINES} ${CXXFLAGS} $<

${OBJECTS}: config.mk

.cpp.o:
	@echo CXX $<
	$(CXX) $(CXXFLAGS) $(DEFINES) -c $< -o $@

.c.o:
	@echo CXX $<
	$(CC) $(CFLAGS) $(DEFINES) $(CUSTOM) -c $< -o $@

slimlock: $(OBJECTS)
	@echo CXX -o $@
	@${CXX} -o $@ $(OBJECTS) ${OBJ} ${LDFLAGS} 

clean:
	@echo cleaning
	@rm -f slimlock ${OBJ} slimlock-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p slimlock-${VERSION}
	@cp -R LICENSE Makefile README config.mk ${SRC} slimlock-${VERSION}
	@tar -cf slimlock-${VERSION}.tar slimlock-${VERSION}
	@gzip slimlock-${VERSION}.tar
	@rm -rf slimlock-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f slimlock ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/slimlock
	@chmod u+s ${DESTDIR}${PREFIX}/bin/slimlock

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/slimlock

.PHONY: all options clean dist install uninstall
