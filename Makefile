DEPENDS="wayland-client" #"ecore ecore-evas evas"
CC=clang

all:
	$(CC) -Wall -g -o textviewer textviewer.c `pkg-config --libs --cflags harfbuzz freetype2 cairo $(DEPENDS)` -lm
	$(CC) -Wall -g -o map map.c pixmanhelper.c -I. `pkg-config --libs --cflags cairo $(DEPENDS) libcurl pixman-1 libpng12` -ljpeg -lm
	$(CC) -Wall -g -o pkgmanager pkgmanager.c `pkg-config --libs --cflags libopkg`
	$(CC) -Wall -g -o wayland wayland.c `pkg-config --libs --cflags cairo  wayland-client xkbcommon` -lm
	$(CC) -Wall -g -o freetype-svg freetype-svg.c `pkg-config --libs --cflags freetype2 cairo ecore ecore-evas evas`

clean:
	rm -rf textviewer map pkgmanager wayland freetype-svg
