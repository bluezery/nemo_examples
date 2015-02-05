CC=gcc -std=c99
FLAGS=`pkg-config --libs --cflags cairo pixman-1 libpng12 wayland-client` -ljpeg -lm 
## ecore ecore-evas evas

all:
	$(CC) -Wall -g -o textviewer textviewer.c $(FLAGS) `pkg-config --libs --cflags harfbuzz freetype2`
	$(CC) -Wall -g -o map map.c pixmanhelper.c -I. $(FLAGS) `pkg-config --libs --cflags libcurl`
	$(CC) -Wall -g -o pkgmanager pkgmanager.c $(LFAGS) `pkg-config --libs --cflags libopkg`
	$(CC) -Wall -g -o wayland wayland.c $(FLAGS) `pkg-config --libs xkbcommon`
	#$(CC) -Wall -g -o freetype-svg freetype-svg.c `pkg-config --libs --cflags freetype2 cairo ecore ecore-evas evas`

clean:
	rm -rf textviewer map pkgmanager wayland freetype-svg
