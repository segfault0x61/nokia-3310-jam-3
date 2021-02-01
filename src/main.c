#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

const char* window_title = "Nokia 3310 Jam 3";
const uint32_t step_size = 16; // 16 ms is approx. 60hz
const int window_width = 84;
const int window_height = 48;
const int resolution_scale = 10;

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        return EXIT_FAILURE;
    }

    SDL_Window* win = SDL_CreateWindow(window_title, 100, 100, window_width * resolution_scale, window_height * resolution_scale, 0);
    if (win == NULL) {
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        return EXIT_FAILURE;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Surface* loading_surf = IMG_Load("res/ball.png");
    SDL_Texture* ball_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    uint32_t last_step_ticks = 0;
    while (1) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }
        }

        uint32_t ticks = SDL_GetTicks();
        if (ticks - last_step_ticks < step_size) {
            continue;
        }
        last_step_ticks = ticks - ticks % step_size;

        SDL_RenderClear(renderer);

        {
            SDL_Rect dest_rect = { .x = 200, .y = 200, .w = 3 * resolution_scale, .h = 3 * resolution_scale };
            SDL_RenderCopy(renderer, ball_texture, NULL, &dest_rect);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    SDL_Quit();

    return EXIT_SUCCESS;
}
