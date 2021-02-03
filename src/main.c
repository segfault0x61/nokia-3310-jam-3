#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const char *window_title = "Nokia 3310 Jam #3";
const uint32_t scale = 10;
const uint32_t screen_width = 84 * scale;
const uint32_t screen_height = 48 * scale;
const uint32_t step_size = 16; // 8 ms is approx. 120Hz, 16 ms approx. 60hz... etc.
const float steps_per_second = (float)(step_size)*0.001f;
const SDL_Color bg_color = { 0xC7, 0xF0, 0xD8, 0xFF };

const float ball_radius = 2.5f * scale;   // pixels
const float player_width = 7.0f * scale;  // pixels
const float player_height = 6.0f * scale; // pixels
const float brick_width = 6.0f * scale;   // pxiels
const float brick_height = 3.0f * scale;  // pixels

const float gravity = 80.0f * scale; // pixels/s/s
const float fast_gravity = 240.0f * scale; // pixels/s/s

const float ball_bounce_vx = 20.0f * scale;                 // pixels/s
const float ball_bounce_vy = 64.0f * scale;                 // pixels/s
const float ball_light_bounce_vx = 8.0f * scale;            // pixels/s
const float player_max_velocity = 30.0f * scale;            // pixels/s
const float player_terminal_velocity = 60.0f * scale;       // pixels/s
const float player_jump_velocity = 50.0f * scale;           // pixels/s
const float player_max_jump_height = 8.0f * player_height;  // pixels

const float ball_bounce_attenuation = 0.95f;
const float jump_release_attenuation = 0.9f;

const float ball_no_bounce_velocity = 12.0f * scale; // pixels/s

const uint32_t coyote_time = 12;        // steps
const uint32_t time_to_buffer_jump = 8; // steps
const uint32_t max_time = 65535;        // steps

const float time_to_max_velocity = 1.8f * scale;       // steps
const float time_to_zero_velocity = 1.8f * scale;      // steps
const float time_to_pivot = 1.2f * scale;              // steps
const float time_to_squash = 1.2f * scale;             // steps
const float time_to_max_jump = 6.4f * scale;           // steps

const float camera_focus_bottom_margin = 12.8f * scale;
const float camera_move_factor = 0.04f;

const uint32_t max_num_bricks = 256;

const char *game_over_text = " press R to restart ";

typedef struct {
    float px, py, vx, vy;
} body_t;

typedef struct {
    float x, y;
} brick_t;

bool check_collision_circle_rect(float, float, float, float, float, float, float);
bool check_collision_rect_rect(float, float, float, float, float, float, float, float);

float accelerate(float);
float decelerate(float);
float pivot(float);

float rand_range(float, float);
float positive_fmod(float, float);

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        return EXIT_FAILURE;
    }

    if (TTF_Init()) {
        return EXIT_FAILURE;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)) {
        return EXIT_FAILURE;
    }

    Mix_Volume(-1, MIX_MAX_VOLUME / 4);

    SDL_Window *win = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, 0);
    if (win == NULL) {
        return EXIT_FAILURE;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        return EXIT_FAILURE;
    }

    SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);

    SDL_Surface *loading_surf;

    loading_surf = IMG_Load("res/ball.png");
    SDL_Texture *ball_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("res/ball_squash.png");
    SDL_Texture *ball_squash_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("res/player.png");
    SDL_Texture *player_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    loading_surf = IMG_Load("res/player_jumping.png");
    SDL_Texture *player_jumping_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    loading_surf = IMG_Load("res/player_fall.png");
    SDL_Texture *player_fall_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("res/brick.png");
    SDL_Texture *brick_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    TTF_Font *font = TTF_OpenFont("res/EffortsPro.ttf", 16.0f * scale);
    assert(font != NULL);

    int glyph_width, glyph_height;
    TTF_SizeText(font, "a", &glyph_width, &glyph_height);
    SDL_Color text_color = { 0x43, 0x52, 0x3D, 0xFF };    
    SDL_Texture *white_number_textures[10];
    SDL_Texture *yellow_number_textures[10];
    for (int i = 0; i < 10; i++) {
        loading_surf = TTF_RenderGlyph_Blended(font, '0' + i, text_color);
        white_number_textures[i] = SDL_CreateTextureFromSurface(renderer, loading_surf);
        loading_surf = TTF_RenderGlyph_Blended(font, '0' + i, text_color);
        yellow_number_textures[i] = SDL_CreateTextureFromSurface(renderer, loading_surf);
    }

    int game_over_text_width, game_over_text_height;
    TTF_SizeText(font, game_over_text, &game_over_text_width, &game_over_text_height);
    loading_surf = TTF_RenderText_Shaded(font, game_over_text, text_color, bg_color);
    SDL_Texture *game_over_text_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    Mix_Chunk *sfx_jump = Mix_LoadWAV("res/jump.wav");
    assert(sfx_jump != NULL);
    Mix_Chunk *sfx_game_over = Mix_LoadWAV("res/game_over.wav");
    assert(sfx_game_over != NULL);
    Mix_Chunk *sfx_bounce_start = Mix_LoadWAV("res/bounce_start.wav");
    assert(sfx_bounce_start != NULL);
    Mix_Chunk *sfx_bounce_end = Mix_LoadWAV("res/bounce_end.wav");
    assert(sfx_bounce_end != NULL);
    Mix_Chunk *sfx_brick_break = Mix_LoadWAV("res/hit.wav");
    assert(sfx_brick_break != NULL);

    int high_score = 0;

init:;    

    srand(time(NULL));
    float start_x = rand_range(12.8f * scale, screen_width - 12.8f * scale);
    float start_y = 12.8f * scale;

    body_t ball = {
        .px = start_x,
        .py = start_y + 25.6f * scale,
    };

    body_t player = {
        .px = start_x - player_width / 2,
        .py = start_y + 12.8f * scale,
    };

    brick_t bricks[max_num_bricks];
    memset(bricks, 0, max_num_bricks * sizeof(brick_t));
    bricks[0].x = start_x - 6.0f * scale;
    bricks[0].y = start_y + 6.0f * scale;
    bricks[1].x = start_x;
    bricks[1].y = start_y + 6.0f * scale;

    float last_x = start_x;
    float last_y = start_y;
    for (int i = 3; i < 255; i += 3) {
        float x = rand_range(0.0f, 1.0f) > 0.5f ? rand_range(last_x + 3.0f * brick_width, last_x + 6.0f * brick_width) : rand_range(last_x - 9.0f * brick_width, last_x - 6.0f * brick_width);
        float y = last_y + rand_range(0.5f * player_height, 1.5f * player_height);
        last_x = x;
        last_y = y;
        bricks[i].x = last_x - brick_width / 2.0f;
        bricks[i].y = last_y;
        bricks[i + 1].x = last_x - brick_width * 3.0f / 2.0f;
        bricks[i + 1].y = last_y;
        bricks[i + 2].x = last_x + brick_width / 2.0f;
        bricks[i + 2].y = last_y;
    }

    int next_brick = 0;

    uint32_t last_step_ticks = 0;
    float last_ball_px = 0.0f;
    float last_ball_py = 0.0f;
    float last_player_px = 0.0f;
    float last_player_py = 0.0f;

    bool left_pressed = false;
    bool right_pressed = false;
    bool down_pressed = false;
    bool player_on_ground = false;
    bool player_carrying_ball = false;
    bool player_jumping = false;
    bool ball_bouncing = false;

    bool left_pressed_entering_carry_state = false;
    bool right_pressed_entering_carry_state = false;

    float player_carry_offset = 0.0f;
    float stored_ball_vx = 0.0f;
    float stored_ball_vy = 0.0f;
    float stored_ball_py = 0.0f;

    uint32_t ball_carry_time = 0;
    uint32_t ball_bounce_time = 0;
    uint32_t air_time = 0;
    uint32_t jump_time = 0;
    uint32_t time_since_jump_press = max_time;
    uint32_t time_since_jump_release = max_time - 1;

    float camera_y = 0.0f;
    float camera_focus_y = bricks[0].y;

    brick_t *player_brick = NULL;
    brick_t *hit_brick = NULL;

    bool game_over = false;
    uint32_t score = 0;

    while (1) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }
            // Ignore key repeats
            if (e.key.repeat) {
                goto after_handle_input;
            }
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                case SDLK_a:
                    left_pressed = true;
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    right_pressed = true;
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    down_pressed = true;
                    break;
                case SDLK_r:
                    goto init;
                    break;
                case SDLK_SPACE:
                    time_since_jump_press = 0;
                    break;
                default:
                    break;
                }
            }
            if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                case SDLK_a:
                    left_pressed = false;
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    right_pressed = false;
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    down_pressed = false;
                    break;
                case SDLK_SPACE:
                    time_since_jump_release = 0;
                    break;
                default:
                    break;
                }
            }
        }
    after_handle_input:;

        if (game_over) {
            continue;
        }

        uint32_t ticks = SDL_GetTicks();
        if (ticks - last_step_ticks < step_size) {
            continue;
        }
        last_step_ticks = ticks - ticks % step_size;

        // Step ball
        last_ball_px = ball.px;
        last_ball_py = ball.py;
        if (!ball_bouncing) {
            ball.vy -= steps_per_second * gravity;
        }
        ball.px += steps_per_second * ball.vx;
        ball.py += steps_per_second * ball.vy;

        // Step player
        last_player_px = player.px;
        last_player_py = player.py;
        if (left_pressed ^ right_pressed) {
            if (left_pressed) {
                if (player.vx > 0.0f) {
                    player.vx = pivot(player.vx);
                } else {
                    player.vx = -accelerate(-player.vx);
                }
            } else {
                if (player.vx < 0.0f) {
                    player.vx = -pivot(-player.vx);
                } else {
                    player.vx = accelerate(player.vx);
                }
            }
        } else {
            if (player.vx > 0.0f) {
                player.vx = decelerate(player.vx);
            } else {
                player.vx = -decelerate(-player.vx);
            }
        }
        // Initiate jump if possible
        if (time_since_jump_press < time_to_buffer_jump) {
            // Jump has just been pressed or is buffered
            if (!player_jumping && (player_on_ground || air_time < coyote_time)) {
                // Player is able to jump
                player.vy = player_jump_velocity;
                player_jumping = true;
                Mix_PlayChannel(-1, sfx_jump, 0);
            }
        }
        if (jump_time > time_to_max_jump) {
            // Max jump has been reached
            player_jumping = false;
        }

        player.vy -= steps_per_second * gravity;
        player.px += steps_per_second * player.vx;
        player.py += steps_per_second * player.vy;

        // Squash ball
        if (player_carrying_ball) {
            ball.py = player.py + player_height + ball_radius;
            if (ball_carry_time < time_to_squash) {
                ball.px = player.px + player_carry_offset;
                ball_carry_time++;
            } else {
                ball.vy = ball_bounce_vy;
                if (left_pressed ^ right_pressed) {
                    if (left_pressed) {
                        if (left_pressed_entering_carry_state) {
                            ball.vx = -ball_bounce_vx;
                        } else {
                            ball.vx = -ball_light_bounce_vx;
                        }
                    } else if (right_pressed) {
                        if (right_pressed_entering_carry_state) {
                            ball.vx = ball_bounce_vx;
                        } else {
                            ball.vx = ball_light_bounce_vx;
                        }
                    }
                } else {
                    ball.vx = 0.0f;
                }
                right_pressed_entering_carry_state = false;
                left_pressed_entering_carry_state = false;
                player_carrying_ball = false;
                ball_carry_time = 0;
                Mix_PlayChannel(-1, sfx_bounce_end, 0);
            }
        }
        if (ball_bouncing) {
            if (ball_bounce_time < time_to_squash) {
                ball_bounce_time++;
            } else {
                ball.vx = stored_ball_vx;
                ball.vy = stored_ball_vy;
                ball.py = stored_ball_py;
                ball_bouncing = false;
                ball_bounce_time = 0;

                // Break brick
                hit_brick->x = 0;
                hit_brick->y = 0;
                hit_brick = NULL;
                Mix_PlayChannel(-1, sfx_brick_break, 0);
                score++;
                if (score > high_score) {
                    high_score = score;
                }                
            }
        }

        // Check if ball falls off the bottom of screen
        if (ball.py + ball_radius < camera_y) {
            game_over = true;
            Mix_PlayChannel(-1, sfx_game_over, 0);
        }

        // Check for collision between ball and player
        if (!player_carrying_ball) {
            bool collision =
                check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                            positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height) ||
                check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                            positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height);
            if (collision && last_ball_py > player.py + player_height && ball.vy < 0) {
                // Enter carry state
                player_carry_offset = ball.px - player.px;
                left_pressed_entering_carry_state = left_pressed;
                right_pressed_entering_carry_state = right_pressed;
                player_carrying_ball = true;
                Mix_PlayChannel(-1, sfx_bounce_start, 0);
            }
        }

        // Check for collision between ball and brick or player and brick
        player_brick = NULL;
        for (int i = 0; i < max_num_bricks; i++) {
            brick_t *brick = &bricks[i];
            if (brick->y + brick_height < camera_y) {
                // Off-screen bricks don't have collision
                continue;
            }
            {
                bool collision =
                    check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                                positive_fmod(brick->x, (float)screen_width), brick->y, brick_width, brick_height) ||
                    check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                                positive_fmod(brick->x, (float)screen_width) - screen_width, brick->y, brick_width, brick_height);
                if (collision && last_ball_py - ball_radius + 0.001f > brick->y + brick_height && ball.vy < 0) {
                    ball.py = brick->y + brick_height + ball_radius;
                    ball_bouncing = true;
                    stored_ball_vx = ball.vx;
                    stored_ball_vy = -ball_bounce_attenuation * ball.vy;
                    ball.vx = 0.0f;
                    ball.vy = 0.0f;
                    stored_ball_py = ball.py;
                    hit_brick = brick;
                    Mix_PlayChannel(-1, sfx_bounce_start, 0);
                }
            }
            {
                bool collision =
                    check_collision_rect_rect(positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height,
                                              positive_fmod(brick->x, (float)screen_width), brick->y, brick_width, brick_height) ||
                    check_collision_rect_rect(positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height,
                                              positive_fmod(brick->x, (float)screen_width) - screen_width, brick->y, brick_width, brick_height);
                if (collision && last_player_py + 0.001f > brick->y + brick_height && player.vy < 0) {
                    camera_focus_y = fmax(camera_focus_y, brick->y);
                    player_brick = brick;
                    player.py = brick->y + brick_height;
                    player.vy = 0.0f;
                    player_on_ground = true;
                    player_jumping = false;
                }
            }
        }

        if (player_brick == NULL) {
            player_on_ground = false;
        }

        // Move camera
        float camera_target_y = camera_focus_y - camera_focus_bottom_margin;
        if (fabs(camera_y - camera_target_y) > 0.001f) {
            camera_y = (1.0f - camera_move_factor) * camera_y + camera_move_factor * camera_target_y;
        }

        // Increment counters
        if (!player_on_ground) {
            air_time++;
            if (player_jumping) {
                jump_time++;
            }
        } else {
            air_time = 0;
            jump_time = 0;
        }

        if (time_since_jump_press < max_time) {
            time_since_jump_press++;
        }
        if (time_since_jump_release < max_time - 1) {
            time_since_jump_release++;
        }

        // Render
        SDL_RenderClear(renderer);
        if (!game_over) {
            for (int i = 0; i < max_num_bricks; i++) {
                brick_t *brick = &bricks[i];
                if (brick->x == 0 && brick->y == 0) {
                    continue;
                }
                SDL_Rect dst_rect = {.x = (int)brick->x, .y = screen_height - (int)(brick->y + brick_height - camera_y), .w = (int)brick_width, .h = (int)brick_height};
                dst_rect.x = positive_fmod(dst_rect.x, screen_width);
                SDL_Rect wrap_rect = dst_rect;
                wrap_rect.x -= screen_width;
                SDL_RenderCopy(renderer, brick_texture, NULL, &dst_rect);
                SDL_RenderCopy(renderer, brick_texture, NULL, &wrap_rect);
            }
            {
                SDL_Rect dst_rect = {.x = (int)(ball.px - ball_radius), .y = screen_height - (int)(ball.py + ball_radius - camera_y), .w = (int)(ball_radius * 2), .h = (int)(ball_radius * 2)};
                if (player_carrying_ball || ball_bouncing) {
                    const int ball_squash_width = 2.0f * ball_radius;
                    float x = ball.px - (float)ball_squash_width / 2.0f;
                    dst_rect.w = ball_squash_width;
                    dst_rect.x = x;
                }
                dst_rect.x = positive_fmod(dst_rect.x, screen_width);
                SDL_Rect wrap_rect = dst_rect;
                wrap_rect.x -= screen_width;            
                if (player_carrying_ball || ball_bouncing) {
                    SDL_RenderCopy(renderer, ball_squash_texture, NULL, &dst_rect);
                    SDL_RenderCopy(renderer, ball_squash_texture, NULL, &wrap_rect);
                } else {
                    SDL_RenderCopy(renderer, ball_texture, NULL, &dst_rect);
                    SDL_RenderCopy(renderer, ball_texture, NULL, &wrap_rect);
                }
            }
            {
                SDL_Rect dst_rect = {.x = (int)player.px, .y = screen_height - (int)(player.py + player_height - camera_y), .w = (int)player_width, .h = (int)player_height};
                dst_rect.x = positive_fmod(dst_rect.x, screen_width);
                SDL_Rect wrap_rect = dst_rect;
                wrap_rect.x -= screen_width;
                if (player_on_ground || air_time < coyote_time) {
                    SDL_RenderCopy(renderer, player_texture, NULL, &dst_rect);
                    SDL_RenderCopy(renderer, player_texture, NULL, &wrap_rect);
                } else {
                    if (player_jumping) {
                        SDL_RenderCopy(renderer, player_jumping_texture, NULL, &dst_rect);
                        SDL_RenderCopy(renderer, player_jumping_texture, NULL, &wrap_rect);
                    } else {
                        SDL_RenderCopy(renderer, player_fall_texture, NULL, &dst_rect);
                        SDL_RenderCopy(renderer, player_fall_texture, NULL, &wrap_rect);
                    }
                }
            }
            {
                int digit = score;
                int i = 0;
                do {
                    SDL_Rect dst_rect = {screen_width - glyph_width * (i + 1), screen_height - 2.0f * glyph_height, glyph_width, glyph_height};
                    SDL_RenderCopy(renderer, white_number_textures[digit % 10], NULL, &dst_rect);
                    digit /= 10;
                    i++;
                } while (digit > 0);
            }
            {
                int digit = high_score;
                int i = 0;
                do {
                    SDL_Rect dst_rect = {screen_width - glyph_width * (i + 1), screen_height - glyph_height, glyph_width, glyph_height};
                    SDL_RenderCopy(renderer, yellow_number_textures[digit % 10], NULL, &dst_rect);
                    digit /= 10;
                    i++;
                } while (digit > 0);
            }            
        }
        if (game_over) {
            SDL_Rect dst_rect = {screen_width * 0.5f - game_over_text_width * 0.5f, screen_height * 0.5f - game_over_text_height * 0.5f, game_over_text_width, game_over_text_height};
            SDL_RenderCopy(renderer, game_over_text_texture, NULL, &dst_rect);
        }
        SDL_RenderPresent(renderer);
    }

    Mix_FreeChunk(sfx_jump);
    Mix_FreeChunk(sfx_game_over);
    Mix_FreeChunk(sfx_bounce_start);
    Mix_FreeChunk(sfx_bounce_end);
    Mix_FreeChunk(sfx_brick_break);
    Mix_CloseAudio();
    Mix_Quit();

    IMG_Quit();

    TTF_CloseFont(font);
    TTF_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    SDL_Quit();

    return 0;
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

float square(float x) {
    return x * x;
}

float quadric(float x) {
    return x * x * x * x;
}

float quadrt(float x) {
    return sqrt(sqrt(x));
}

float quintic(float x) {
    return x * x * x * x * x;
}

float quintic_root(float x) {
    return pow(x, .2f);
}

float identity(float x) {
    return x;
}

// Return a value larger than or equal to velocity (positive values only)
float accelerate(float velocity) {
    return fmin(player_max_velocity, player_max_velocity * square(sqrt(velocity / player_max_velocity) + 1.0f / time_to_max_velocity));
}

// Return a value less than velocity that approaches zero (positive values only)
float decelerate(float velocity) {
    return fmax(0.0f, velocity - player_max_velocity / time_to_zero_velocity);
}

// Return a value less than velocity that approaches zero (positive values only)
float pivot(float velocity) {
    return fmax(0.0f, velocity - player_max_velocity / time_to_pivot);
}

float rand_range(float min, float max) {
    float r = (float)rand() / (float)RAND_MAX;
    return min + r * (max - min);
}

float positive_fmod(float x, float mod) {
    float xm = fmod(x, mod);
    if (xm < 0) {
        return xm + mod;
    }
    return xm;
}
