#ifndef DRAW_H
#define DRAW_H
#include <stdint.h>

struct state{
    int width;
    int height;
    uint32_t *buf;
};
typedef struct state* context;

context create_ctx(uint32_t *buf, int width, int height);
void destroy_ctx(context ctx);
void fill_rec(context ctx, float x, float y, float width, float height, int color);
#endif
