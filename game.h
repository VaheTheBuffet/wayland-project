#ifndef GAME_H
#define GAME_H
#include <stdint.h>

typedef enum Direction {
    UP, DOWN, RIGHT, LEFT
} Direction;

typedef struct IVec2 {
    uint8_t x;
    uint8_t y;
} IVec2;
#define IVEC2_NULL(vec) (vec.x == 0xFF)

typedef struct Game {
    IVec2 *snake;
    bool *keystate;
    IVec2 apples[2];
    int snake_len;
    bool running;
    bool ate;

    int board_width;
    int board_height;

    Direction direction;
} Game;

//assumes 128 keys
Game* Game_init(bool *keystate);
void Game_free(Game *game);

void Game_update(Game *game);
void update_move(Game *game);
void update_direction(Game *game);
void update_eat(Game *game);
void update_lose(Game *game);
void update_apples(Game *game);
#endif
