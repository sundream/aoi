all:
	gcc -o aoi -g -Wall aoi.c test.c -lm \
		-Wno-int-to-pointer-cast -Wno-pointer-to-int-cast
	#gcc -o aoi -g -Wall aoi.c test.c -lm -DUSE_IN_SKYNET

clean:
	rm -f aoi

.PHONY: all clean
