PREFIX = /usr/local
CC = gcc
INCS = -I/usr/include/freetype2
LIBS = -lX11 -lXft -lfontconfig

SRC = main.c
OBJ = ${SRC:.c=.o}

all: beamwm

.c.o:
	${CC} -c ${INCS} $<

beamwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LIBS}

clean:
	rm -f beamwm ${OBJ}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f beamwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/beamwm
	mkdir -p ${DESTDIR}${PREFIX}/share/xsessions
	echo "[Desktop Entry]" > beamwm.desktop
	echo "Name=beamwm" >> beamwm.desktop
	echo "Comment=Minimalist tiling WM" >> beamwm.desktop
	echo "Exec=beamwm" >> beamwm.desktop
	echo "Type=Application" >> beamwm.desktop
	cp -f beamwm.desktop ${DESTDIR}${PREFIX}/share/xsessions/
	chmod 644 ${DESTDIR}${PREFIX}/share/xsessions/beamwm.desktop

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/beamwm
	rm -f ${DESTDIR}${PREFIX}/share/xsessions/beamwm.desktop

.PHONY: all clean install uninstall
