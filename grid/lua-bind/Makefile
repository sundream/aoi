all : laoi.so

laoi.so: laoi.c ../src/aoi.c
	gcc -fPIC --shared -g -Wall -lm -I/usr/local/include -I../src/ -L/usr/local/lib -o $@ $^ \
		-Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

test:
	lua test.lua

clean:
	rm -f laoi.so

.PHONY: all clean test
