#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

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

const float ball_radius = 1.0f * scale;  // pixels
const float player_width = 2.f * scale;  // pixels
const float player_height = 2.f * scale; // pixels
const float brick_width = 6.4f * scale;  // pxiels
const float brick_height = 1.6f * scale; // pixels

const float gravity = 80.0f * scale; // pixels/s/s
const float fast_gravity = 240.0f * scale; // pixels/s/s

const float ball_bounce_vx = 20.0f * scale;                 // pixels/s
const float ball_bounce_vy = 60.0f * scale;                 // pixels/s
const float player_max_velocity = 30.0f * scale;            // pixels/s
const float player_terminal_velocity = 60.0f * scale;       // pixels/s
const float player_max_jump_height = 8.0f * player_height;  // pixels

const float ball_bounce_attenuation = 1.0f;
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

typedef struct {
    float px, py, vx, vy;
} body_t;

typedef struct {
    float x, y;
} brick_t;

bool check_collision_circle_rect(float, float, float, float, float, float, float);
bool check_collision_rect_rect(float, float, float, float, float, float, float, float);

float jump_velocity(float);
float fall_velocity(float);
float accelerate(float);
float decelerate(float);
float pivot(float);

float rand_range(float, float);
float positive_fmod(float, float);

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        return EXIT_FAILURE;
    }

    SDL_Window *win = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, 0);
    if (win == NULL) {
        return EXIT_FAILURE;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        return EXIT_FAILURE;
    }

    SDL_Surface *loading_surf;

    loading_surf = IMG_Load("res/ball.png");
    SDL_Texture *ball_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("res/player.png");
    SDL_Texture *player_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    loading_surf = IMG_Load("res/player_jumping.png");
    SDL_Texture *player_jumping_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("res/brick.png");
    SDL_Texture *brick_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

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
    // bricks[0].x = start_x - 6.4f * scale;
    // bricks[0].y = start_y + 6.4f * scale;
    // bricks[1].x = start_x;
    // bricks[1].y = start_y + 6.4f * scale;
    
    bricks[0].x = start_x - brick_width / 2.0f;
    bricks[0].y = start_y;
    bricks[1].x = start_x - brick_width * 3.0f / 2.0f;
    bricks[1].y = start_y;
    bricks[2].x = start_x + brick_width / 2.0f;
    bricks[2].y = start_y;

    bricks[3].x = start_x - brick_width / 2.0f + 12.8f * scale;
    bricks[3].y = start_y + 12.8f * scale;
    bricks[4].x = start_x - brick_width * 3.0f / 2.0f + 12.8f * scale;
    bricks[4].y = start_y + 12.8f * scale;
    bricks[5].x = start_x + brick_width / 2.0f + 12.8f * scale;
    bricks[5].y = start_y + 12.8f * scale;

    uint32_t last_step_ticks = 0;
    float last_ball_px = 0.0f;
    float last_ball_py = 0.0f;
    float last_player_px = 0.0f;
    float last_player_py = 0.0f;
    brick_t *hit_brick1 = NULL;
    brick_t *hit_brick2 = NULL;

    bool left_pressed = false;
    bool right_pressed = false;
    bool down_pressed = false;
    bool player_on_ground = false;
    bool player_carrying_ball = false;
    bool player_jumping = false;
    bool ball_bouncing = false;

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

        uint32_t ticks = SDL_GetTicks();
        if (ticks - last_step_ticks < step_size) {
            continue;
        }
        last_step_ticks = ticks - ticks % step_size;

        // Step ball
        last_ball_px = ball.px;
        last_ball_py = ball.py;
        ball.vy -= steps_per_second * gravity;
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
        // Try to jump
        if (time_since_jump_press < time_to_buffer_jump) {
            // Jump has just been pressed/triggered
            if (player_on_ground || air_time < coyote_time) {
                // Player is able to jump
                player.vy = player_max_jump_height * jump_velocity(0.0f);
                player_jumping = true;
            }
        }
        if (jump_time > time_to_max_jump) {
            // Max jump has been reached
            player_jumping = false;
        }
        if (player_jumping) {
            player.vy = player_max_jump_height * jump_velocity((float)jump_time / time_to_max_jump);
        } else {
            if (down_pressed) {
                player.vy = fmax(-player_terminal_velocity, player.vy - steps_per_second * fast_gravity);
            } else {
                player.vy = fmax(-player_terminal_velocity, player.vy - steps_per_second * gravity);
            }
        }

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
                    ball.vx = left_pressed ? -ball_bounce_vx : ball_bounce_vx;
                } else {
                    ball.vx = 0.0f;
                }
                player_carrying_ball = false;
                ball_carry_time = 0;
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

                if (hit_brick1 != NULL) {
                    hit_brick1->x = 0;
                    hit_brick1->y = 0;
                    hit_brick1 = NULL;
                }
                if (hit_brick2 != NULL) {
                    hit_brick2->x = 0;
                    hit_brick2->y = 0;
                    hit_brick2 = NULL;
                }                
            }
        }
        
        // Check for collision between ball and player
        {
                bool collision =
                check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                            positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height) ||
                check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                            positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height);            if (collision && last_ball_py > player.py + player_height && ball.vy < 0) {
                // Enter carry state
                player_carry_offset = ball.px - player.px;
                player_carrying_ball = true;
            }
        }

        // Check for collision between ball and player.
        {
            bool collision =
                check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                            positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height) ||
                check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                            positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height);
            if (collision && last_ball_py > player.py + player_height && ball.vy < 0) {
                // Enter carry state.
                player_carry_offset = ball.px - player.px;
                player_carrying_ball = true;
            }
        }

        // Check for collision between ball and brick or player and brick
        player_brick = NULL;
        for (int i = 0; i < max_num_bricks; i++) {
            brick_t *brick = &bricks[i];
            {
                bool collision =
                    check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                                positive_fmod(brick->x, (float)screen_width), brick->y, brick_width, brick_height) ||
                    check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                                positive_fmod(brick->x, (float)screen_width) - screen_width, brick->y, brick_width, brick_height);
                if (collision && last_ball_py - ball_radius + 0.001f > brick->y + brick_height && ball.vy < 0) {
                    ball.py = brick->y + brick_height + ball_radius;
                    if (ball.vy > -ball_no_bounce_velocity) {
                        ball.vy = 0.0f;
                    } else {
                        ball_bouncing = true;
                        stored_ball_vx = ball.vx;
                        stored_ball_vy = -ball_bounce_attenuation * ball.vy;
                        ball.vx = 0.0f;
                        ball.vy = 0.0f;
                        stored_ball_py = ball.py;
                        if (hit_brick1 == NULL) {
                            hit_brick1 = brick;
                        } else if (hit_brick2 == NULL) {
                            hit_brick2 = brick;
                        }
                    }
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
        SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        SDL_RenderClear(renderer);
        for (int i = 0; i < max_num_bricks; i++) {
            brick_t *brick = &bricks[i];
            SDL_Rect dst_rect = {.x = (int)brick->x, .y = screen_height - (int)(brick->y + brick_height - camera_y), .w = (int)brick_width, .h = (int)brick_height};
            dst_rect.x = positive_fmod(dst_rect.x, screen_width);
            SDL_Rect wrap_rect = dst_rect;
            wrap_rect.x -= screen_width;
            SDL_RenderCopy(renderer, brick_texture, NULL, &dst_rect);
            SDL_RenderCopy(renderer, brick_texture, NULL, &wrap_rect);
        }
        {
            SDL_Rect dst_rect = {.x = (int)(ball.px - ball_radius), .y = screen_height - (int)(ball.py + ball_radius - camera_y), .w = (int)(ball_radius * 2), .h = (int)(ball_radius * 2)};
            dst_rect.x = positive_fmod(dst_rect.x, screen_width);
            SDL_Rect wrap_rect = dst_rect;
            wrap_rect.x -= screen_width;            
            if (player_carrying_ball) {
                float pct = (float)ball_carry_time / (float)time_to_squash;
                pct *= 2;
                pct -= 1;
                pct = pct * pct * pct * pct;
                pct = 1 - pct;
                dst_rect.x -= pct * ball_radius / 2;
                dst_rect.w += pct * ball_radius;
                dst_rect.y += pct * ball_radius * 5 / 8;
                dst_rect.h -= pct * ball_radius * 5 / 8;
            }
            if (ball_bouncing) {
                float pct = (float)ball_bounce_time / (float)time_to_squash;
                pct *= 2;
                pct -= 1;
                pct = pct * pct * pct * pct;
                pct = 1 - pct;
                dst_rect.x -= pct * ball_radius / 2;
                dst_rect.w += pct * ball_radius;
                dst_rect.y += pct * ball_radius * 5 / 8;
                dst_rect.h -= pct * ball_radius * 5 / 8;
            }            
            SDL_RenderCopy(renderer, ball_texture, NULL, &dst_rect);
            SDL_RenderCopy(renderer, ball_texture, NULL, &wrap_rect);
        }
        {
            SDL_Rect dst_rect = {.x = (int)player.px, .y = screen_height - (int)(player.py + player_height - camera_y), .w = (int)player_width, .h = (int)player_height};
            dst_rect.x = positive_fmod(dst_rect.x, screen_width);
            SDL_Rect wrap_rect = dst_rect;
            wrap_rect.x -= screen_width;
            if (player_jumping) {
                SDL_RenderCopy(renderer, player_jumping_texture, NULL, &dst_rect);
                SDL_RenderCopy(renderer, player_jumping_texture, NULL, &wrap_rect);
            } else {
                SDL_RenderCopy(renderer, player_texture, NULL, &dst_rect);
                SDL_RenderCopy(renderer, player_texture, NULL, &wrap_rect);
            }
        }
        SDL_RenderPresent(renderer);
    }

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

// Jump function, describes jumping velocity over time over the domain of
// [0, 1] where 1 is the top of the jump
float jump_velocity(float x) {
    return -2.0f * (x - 1.0f);
}

// Fall function, describes falling velocity over time over the domain of
// [0, 1] where 1 is the point at which the ground would be reached if falling
// from maximum jump height
float fall_velocity(float x) {
    return 3.0f * pow(x, 2.0f);
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
