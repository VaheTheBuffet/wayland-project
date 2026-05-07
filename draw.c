#include "draw.h"
#include <stdlib.h>

context create_ctx(uint32_t *buf, int width, int height) 
{
    context ctx = (context)malloc(sizeof(struct state));
    ctx->width = width;
    ctx->height = height;
    ctx->buf = buf;

    return ctx;
}

void destroy_ctx(context ctx) 
{
    free(ctx);
}

void fill_rec(context ctx, float x, float y, float width, float height, int color) 
{
    int x_rounded = x;
    int y_rounded = y;

    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            ctx->buf[x_rounded + i + (y_rounded + j) * ctx->width] = color;
        }
    }
}
