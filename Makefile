RELEASE_NAME = uphill-break-0.4
BINARY_NAME = uphill-break

SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LIBS = $(shell sdl2-config --libs)

CC ?= gcc

$(BINARY_NAME): ./src/main.c Makefile
	$(CC) -o $@ -lm $(SDL2_CFLAGS) $(SDL2_LIBS) -lSDL2_mixer -lSDL2_image -lSDL2_ttf $< -Wl,-rpath='$${ORIGIN}/lib'

tar: $(BINARY_NAME)
	rm -rf $@
	tar --dereference \
		--transform 's|^|$(RELEASE_NAME)/|' \
		--transform 's|^usr/lib/|bin/lib/|' \
		--transform 's|$(BINARY_NAME)|bin/$(BINARY_NAME)|' \
		-czf $(RELEASE_NAME).tar.gz \
			$(wildcard res/*.ttf) \
			$(wildcard res/*.png) \
			$(wildcard res/*.wav) \
			/usr/lib/libSDL2-2.0.so.0 \
			/usr/lib/libSDL2_image-2.0.so.0 \
			/usr/lib/libSDL2_mixer-2.0.so.0 \
			/usr/lib/libSDL2_ttf-2.0.so.0 \
			$(BINARY_NAME) \
			start

clean:
	rm -f bin/uphill-break

.PHONY: clean tar
