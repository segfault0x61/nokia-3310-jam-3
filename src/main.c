#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

const char* window_title = "Nokia 3310 Jam 3";
const uint32_t step_size = 8; // 8 ms is approx. 120hz
const int resolution_scale = 10;
const int window_width = 84 * resolution_scale;
const int window_height = 48 * resolution_scale;
const float steps_per_second = (float)(step_size)*0.001f;
const SDL_Color bg_color = { 0xC7, 0xF0, 0xD8, 0xFF };

const uint8_t ball_radius = 1.6 * resolution_scale;
const float player_width = 3.2f * resolution_scale;
const float player_height = 3.2f * resolution_scale;
const float player_speed = 300.0f; // pixels/s

typedef struct {
    float px, py, vx, vy;
} body_t;

bool check_collision_circle_rect(float, float, float, float, float, float, float);
bool check_collision_rect_rect(float, float, float, float, float, float, float, float);

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        return EXIT_FAILURE;
    }

    SDL_Window* win = SDL_CreateWindow(window_title, 100, 100, window_width, window_height, 0);
    if (win == NULL) {
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        return EXIT_FAILURE;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Surface* loading_surf;
    loading_surf = IMG_Load("res/ball.png");

    SDL_Texture* ball_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("res/guy.png");
    SDL_Texture *player_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    body_t ball = {
        .px = 300.0f,
        .py = 200.0f,
        .vx = 200.0f,
        .vy = 0.0f,
    };
    body_t player = {
        .px = 600.0f,
        .py = 400.0f,
        .vx = 0.0f,
        .vy = 0.0f,
    };

    const uint8_t num_bodies = 2;
    body_t *bodies[] = {&ball, &player};

    uint32_t last_step_ticks = 0;

    bool left_pressed = false;
    bool right_pressed = false;

    while (1) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                    left_pressed = true;
                    break;
                case SDLK_RIGHT:
                    right_pressed = true;
                    break;
                default:
                    break;
                }
            }
            if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                    left_pressed = false;
                    break;
                case SDLK_RIGHT:
                    right_pressed = false;
                    break;
                default:
                    break;
                }
            }
        }

        uint32_t ticks = SDL_GetTicks();
        if (ticks - last_step_ticks < step_size) {
            continue;
        }
        last_step_ticks = ticks - ticks % step_size;

        // Step ball.
        ball.vy += steps_per_second * 400.0f;
        ball.px += steps_per_second * ball.vx;
        ball.py += steps_per_second * ball.vy;
        if (ball.py + ball_radius > window_height) {
            ball.vy *= -1.0f;
        }
        if (ball.px + ball_radius > window_width) {
            ball.vx *= -1.0f;
        }
        if (ball.px - ball_radius < 0) {
            ball.vx *= -1.0f;
        }

        // Step player.
        if (left_pressed ^ right_pressed) {
            player.vx = left_pressed ? -player_speed : player_speed;
        } else {
            player.vx = 0;
        }
        player.vy += steps_per_second * 400.0f;
        player.px += steps_per_second * player.vx;
        player.py += steps_per_second * player.vy;

        if (player.py + player_height > window_height) {
            player.vy = 0;
            player.py = window_height - player_height;
        }

        // Check for collision between ball and player.
        bool collision = check_collision_circle_rect(ball.px, ball.py, ball_radius, player.px, player.py, player_width, player_height);
        if (collision) {
            printf("collision!\n");
        }

        // Render.
        SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        SDL_RenderClear(renderer);
        {
            SDL_Rect dst_rect = {.x = (int)(ball.px - ball_radius), .y = (int)(ball.py - ball_radius), .w = (int)(ball_radius * 2), .h = (int)(ball_radius * 2)};
            SDL_RenderCopy(renderer, ball_texture, NULL, &dst_rect);
        }
        {
            SDL_Rect dst_rect = {.x = (int)player.px, .y = (int)player.py, .w = (int)player_width, .h = (int)player_height};
            SDL_RenderCopy(renderer, player_texture, NULL, &dst_rect);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    SDL_Quit();

    return EXIT_SUCCESS;
}

bool check_collision_rect_rect(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    bool x = bx <= ax + aw && ax <= bx + bw;
    bool y = by <= ay + ah && ay <= by + bh;
    return x && y;
}

bool check_collision_circle_rect(float cx, float cy, float cr, float rx, float ry, float rw, float rh) {
    if (!check_collision_rect_rect(cx - cr, cy - cr, 2 * cr, 2 * cr, rx, ry, rw, rh)) {
        return false;
    }

    // Check which of 9 zones the circle is in:
    //
    //    top left | top    | top right
    // -----------------------------------
    //        left | rect   | right
    // -----------------------------------
    // bottom left | bottom | bottom right
    //
    // rect, left, top, right, bottom: definitely colliding
    // top left, top right, bottom left, bottom right: maybe, but need to further check if a corner of the rect is contained in the circle

    // Short-circuit for rect, left, top, right, bottom
    if (cx < rx) {
        if (ry <= cy && cy < ry + rh) {
            return true;
        }
    } else if (cx < rx + rw) {
        return true;
    } else {
        if (ry <= cy && cy < ry + rh) {
            return true;
        }
    }

    // Extra check for corner containment in case circle is in a diagonal zone
    float d0 = (rx - cx) * (rx - cx) + (ry - cy) * (ry - cy);
    float d1 = (rx + rw - cx) * (rx + rw - cx) + (ry - cy) * (ry - cy);
    float d2 = (rx - cx) * (rx - cx) + (ry + rh - cy) * (ry + rh - cy);
    float d3 = (rx + rw - cx) * (rx + rw - cx) + (ry + rh - cy) * (ry + rh - cy);
    float rr = cr * cr;

    return d0 < rr || d1 < rr || d2 < rr || d3 < rr;
}