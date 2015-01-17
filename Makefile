all:
	clang -Wall -g -o show show.c `pkg-config --libs --cflags harfbuzz freetype2 cairo ecore ecore-evas evas` -lm
	clang -Wall -g -o freetype-svg freetype-svg.c `pkg-config --libs --cflags freetype2` 

clean:
	rm -rf hb-test show
