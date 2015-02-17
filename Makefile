CC=clang #gcc -std=c99q

PKGS=cairo pixman-1 libpng12 wayland-client harfbuzz freetype2 fontconfig
CFLAGS=-Wall -fvisibility=hidden -fPIC -DEAPI=__attribute__\(\(visibility\(\"default\"\)\)\)
CFLAGS:=$(CFLAGS) `pkg-config --cflags $(PKGS)` 
LDFLAGS=-Wl,-z,defs -Wl,--as-needed -Wl,--hash-style=both
LDFLAGS:=$(LDFLAGS) -lm -lrt -ljpeg `pkg-config --libs $(PKGS)`

## ecore ecore-evas evas
LIB=util.o cairo_view.o wl_window.o pixmanhelper.o view.o text.o

all: textviewer map pkgmanager wayland

textviewer: textviewer.c  $(LIB)
	$(CC) -g -o $@ $@.c $(LIB) $(CFLAGS) $(LDFLAGS)

map: map.c $(LIB)
	$(CC) -g -o $@ $@.c $(LIB) $(CFLAGS) $(LDFLAGS) `pkg-config --cflags --libs libcurl`

pkgmanager: pkgmanager.c $(LIB)
	$(CC) -g -o $@ $@.c $(LIB) $(CFLAGS) $(LDFLAGS) `pkg-config --cflags --libs libopkg`

wayland: wayland.c $(LIB)
	$(CC) -g -o $@ $@.c $(LIB) $(CFLAGS) $(LDFLAGS) `pkg-config --cflags --libs xkbcommon`
	#$(CC) -Wall -g -o freetype-svg freetype-svg.c `pkg-config --libs --cflags freetype2 cairo ecore ecore-evas evas`

%.o: %.c %.h
	$(CC) -c $*.c $(CFLAGS)

clean:
	rm -rf textviewer map pkgmanager wayland freetype-svg
	rm -rf $(LIB)
