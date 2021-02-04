BINARY_NAME = uphill-break
RELEASE_NAME = $(BINARY_NAME)-$(shell git describe --tags)

SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LIBS = $(shell sdl2-config --libs)

CC ?= gcc

$(BINARY_NAME): ./src/main.c 
	$(CC) -o $@ $< -lm $(SDL2_CFLAGS) $(SDL2_LIBS) -lSDL2_mixer -lSDL2_ttf -lSDL2_image -Wl,-rpath='$${ORIGIN}/lib'

linux: $(BINARY_NAME)

$(RELEASE_NAME)-linux-x86_64.tar.gz: $(BINARY_NAME)
	rm -rf $@
	tar --dereference \
		--transform 's|^|$(RELEASE_NAME)/|' \
		--transform 's|$(BINARY_NAME)$$|bin/$(BINARY_NAME)|' \
		--transform 's|usr|bin|' \
		--transform 's|pkg/||' \
		-czf $(RELEASE_NAME).tar.gz \
			$(wildcard res/*.ttf) \
			$(wildcard res/*.png) \
			$(wildcard res/*.wav) \
			/usr/local/lib/libSDL2-2.0.so.0 \
			/usr/local/lib/libSDL2_image-2.0.so.0 \
			/usr/local/lib/libSDL2_mixer-2.0.so.0 \
			/usr/local/lib/libSDL2_ttf-2.0.so.0 \
			$(BINARY_NAME) \
			pkg/start \
			pkg/README

linuxtar: $(RELEASE_NAME)-linux-x86_64.tar.gz

index.html index.wasm index.data index.js: ./src/main.c
	emcc $< \
		-s USE_SDL=2 \
		-s USE_SDL_IMAGE=2 \
		-s USE_SDL_MIXER=2 \
		-s USE_SDL_TTF=2 \
		-s SDL2_IMAGE_FORMATS='["png"]' \
		-o index.html --preload-file res --shell-file ./web/shell.html

web: index.html index.wasm index.data index.js

$(RELEASE_NAME)-web.zip: index.html index.wasm index.data index.js
	zip $@ $^

webzip: $(RELEASE_NAME)-web.zip

clean:
	rm -f $(BINARY_NAME)
	rm -f $(BINARY_NAME)-*.tar.gz
	rm -f $(BINARY_NAME)-*-web.zip
	rm -f index.html index.wasm index.js index.data

.PHONY: clean tar web webzip