#include <stdlib.h>
#include <string.h>
#include "game.h"
#include <xkbcommon/xkbcommon.h>

Game *Game_init(bool *keystate) 
{
    int board_width = 20;
    int board_height = 20;

    Game *game = malloc(sizeof(Game));
    game->keystate = keystate;
    game->snake_len = 1;
    game->snake = malloc(board_width * board_height * sizeof(IVec2));
    game->snake[0] = (IVec2){10, 10};
    game->board_height = board_height;
    game->board_width = board_width;
    game->apples[0] =  (IVec2){-1, -1};
    game->apples[1] =  (IVec2){-1, -1};
    game->running = true;
    game->direction = RIGHT;

    return game;
}

void Game_free(Game *game)
{
    free(game->snake);
    free(game);
}

void Game_update(Game *game)
{
    update_move(game);
    update_eat(game);
    update_lose(game);
    update_apples(game);
}

void update_direction(Game *game)
{
    if(game->keystate[XKB_KEY_w] && game->direction != DOWN) {
        game->direction = UP;
    }
    if(game->keystate[XKB_KEY_a] && game->direction != RIGHT) {
        game->direction = LEFT;
    }
    if(game->keystate[XKB_KEY_s] && game->direction != UP) {
        game->direction = DOWN;
    }
    if(game->keystate[XKB_KEY_d] && game->direction != LEFT) {
        game->direction = RIGHT;
    }
}

void update_move(Game *game)
{
    if (game->ate) {
        game->snake[game->snake_len] = game->snake[game->snake_len - 1];
        game->snake_len++;
    } else {
        for(int segment = 0; segment < game->snake_len - 1; segment++) {
            game->snake[segment] = game->snake[segment + 1];
        }
    }

    switch(game->direction) {
        case UP:
            game->snake[game->snake_len - 1].y--;
            break;
        case DOWN:
            game->snake[game->snake_len - 1].y++;
            break;
        case RIGHT:
            game->snake[game->snake_len - 1].x++;
            break;
        case LEFT:
            game->snake[game->snake_len - 1].x--;
            break;
    }

    game->ate = false;
}

void update_eat(Game *game)
{
    for(int i = 0; i < sizeof(game->apples) / sizeof(game->apples[0]); i++) {
        if(!memcmp(&game->apples[i], &game->snake[game->snake_len - 1], sizeof(IVec2))) {
            game->ate = true;
            game->apples[i] = (IVec2){-1, -1};
        }
    }
}

void update_lose(Game *game)
{

    //check win condition
    if(game->snake_len == game->board_width * game->board_height) {
        game->running = false;
        return;
    }

    //check bounds
    IVec2 head_location = game->snake[game->snake_len - 1];
    if (
        head_location.x == -1 || head_location.x == game->board_width
        ||head_location.y == -1 || head_location.y == game->board_height
    ){

        game->running = false;
        return;
    }

    // Check intersection
    for(int i = 0; i < game->snake_len-1; i++) {
        if(!memcmp(&head_location, &game->snake[i], sizeof(IVec2))) {
            game->running = false;
            return;
        }
    }
}

void update_apples(Game *game)
{
    for (int i = 0; i < sizeof(game->apples) / sizeof(game->apples[0]); i++) {
        if (IVEC2_NULL(game->apples[i]))  {
            game->apples[i] = (IVec2) {rand() % game->board_width, rand() % game->board_height};
        }
    }
}
