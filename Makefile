CC=gcc -fdiagnostics-color=auto

PKGS=cairo pixman-1 libpng12 wayland-client harfbuzz freetype2 fontconfig nemotale nemotool
CFLAGS=-Wall -fvisibility=hidden -fPIC -DEAPI=__attribute__\(\(visibility\(\"default\"\)\)\)
CFLAGS:=$(CFLAGS) -Iasst/ `pkg-config --cflags $(PKGS)` 
LDFLAGS=-Wl,-z,defs -Wl,--as-needed -Wl,--hash-style=both
LDFLAGS:=$(LDFLAGS) -lm -lrt -ljpeg `pkg-config --libs $(PKGS)`

ASST=mischelper glhelper fbohelper
## ecore ecore-evas evas
LIB=util.o talehelper.o cairo_view.o wl_window.o view.o text.o #$(ASST)

all: nemoeffect 
	#nemotest textviewer

#asst:
#	for i in $(ASST) \
#		$(CC) -g -c asst/$i.c $(CFLAGS)

nemotest_egl: nemotest_egl.c $(LIB)
	$(CC) -g -c $@.c $(CFLAGS)
	$(CC) -g -o $@ $@.o $(LIB) $(LDFLAGS)

nemotest: nemotest.c $(LIB)
	$(CC) -g -c $@.c $(CFLAGS)
	$(CC) -g -o $@ $@.o $(LIB) $(LDFLAGS)

nemoeffect: nemoeffect.c $(LIB)
	$(CC) -g -c $@.c $(CFLAGS)
	$(CC) -g -o $@ $@.o $(LIB) $(LDFLAGS)

textviewer: textviewer.c  $(LIB)
	$(CC) -g -c $@.c $(CFLAGS)
	$(CC) -g -o $@ $@.o $(LIB) $(LDFLAGS)

map: map.c $(LIB)
	$(CC) -g -o $@ $@.c $(LIB) $(CFLAGS) $(LDFLAGS) `pkg-config --cflags --libs libcurl`

pkgmanager: pkgmanager.c $(LIB)
	$(CC) -g -o $@ $@.c $(LIB) $(CFLAGS) $(LDFLAGS) `pkg-config --cflags --libs libopkg`

wayland: wayland.c $(LIB)
	$(CC) -g -o $@ $@.c $(LIB) $(CFLAGS) $(LDFLAGS) `pkg-config --cflags --libs xkbcommon`
	#$(CC) -Wall -g -o freetype-svg freetype-svg.c `pkg-config --libs --cflags freetype2 cairo ecore ecore-evas evas`

%.o: %.c %.h
	$(CC) -g -c $*.c $(CFLAGS)

clean:
	rm -rf nemotest nemoeffect textviewer #map pkgmanager wayland freetype-svg
	rm -rf $(LIB)
