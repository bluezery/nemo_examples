all:
	clang -Wall -g -o show show.c `pkg-config --libs --cflags harfbuzz freetype2 cairo ecore ecore-evas evas wayland-client` -lm
	clang -Wall -g -o map map.c pixmanhelper.c -I. `pkg-config --libs --cflags cairo ecore ecore-evas evas libcurl pixman-1 libpng12` -ljpeg
	clang -Wall -g -o pkgmanager pkgmanager.c `pkg-config --libs --cflags libopkg`
	clang -Wall -g -o freetype-svg freetype-svg.c `pkg-config --libs --cflags freetype2 cairo ecore ecore-evas evas`

clean:
	rm -rf show map freetype-svg
