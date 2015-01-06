all:
	clang -Wall -g -o hb-test hb-test.c `pkg-config --libs --cflags harfbuzz freetype2 glib-2.0 cairo` -lm
	clang -Wall -g -o show show.c `pkg-config --libs --cflags harfbuzz freetype2 glib-2.0 cairo ecore ecore-evas evas` -lm

clean:
	rm -rf hb-test
