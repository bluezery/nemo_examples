CC=gcc -fdiagnostics-color=auto

PKGS=cairo pixman-1 libpng12 wayland-client harfbuzz freetype2 fontconfig nemotale nemotool
CFLAGS=-Wall -fvisibility=hidden -fPIC -DEAPI=__attribute__\(\(visibility\(\"default\"\)\)\)
CFLAGS:=$(CFLAGS) -Iasst/ `pkg-config --cflags $(PKGS)` 
LDFLAGS=-Wl,-z,defs -Wl,--as-needed -Wl,--hash-style=both
LDFLAGS:=$(LDFLAGS) -lm -lrt -ljpeg `pkg-config --libs $(PKGS)`

ASST=mischelper glhelper fbohelper
## ecore ecore-evas evas
LIB=util.o talehelper.o cairo_view.o wl_window.o view.o text.o #$(ASST)

TEST=future2 #future nemoeffect
	#nemoeffect nemotest

all: $(LIB) test
	#textviewer

test:
	@for i in $(TEST); do \
		$(CC) -g -c $$i.c $(CFLAGS) && \
		$(CC) -g -o $$i $$i.o $(LIB) $(LDFLAGS) && \
		echo "Compiled $$i"; \
	done;

nemoeffect:
	$(CC) -g -c $$i.c $(CFLAGS)
	$(CC) -g -o $$i $$i.o $(LIB) $(LDFLAGS)

future2:
	$(CC) -g -c $$i.c $(CFLAGS)
	$(CC) -g -o $$i $$i.o $(LIB) $(LDFLAGS)

future:
	$(CC) -g -c $$i.c $(CFLAGS)
	$(CC) -g -o $$i $$i.o $(LIB) $(LDFLAGS)

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
	rm -rf $(TEST) textviewer #map pkgmanager wayland freetype-svg
	rm -rf $(LIB)
