#ifndef BRUMA_DRAW_H
#define BRUMA_DRAW_H

#include "game.h"

typedef enum { MENU, PLAY, BAG, PAUSE } Screen;

typedef struct {
    Screen screen;
    const char *seed_text;
    const char *dimensions[3];
    const char *message;
    int item_cursor, recipe_cursor, menu_cursor;
    int input_field;
    bool crafting, existing, valid_size;
    float animation;
} View;

bool draw_init(const char *asset_dir);
void draw_frame(Game *game, const View *view, int width, int height);
void draw_close(void);
bool draw_capture(const char *path, int width, int height);
unsigned draw_error(void);
#define VIEW_DISTANCE_MIN 32
#define VIEW_DISTANCE_MAX 104
#define VIEW_DISTANCE_DEFAULT 64   /* la de serie fuera de SaguiOS (en SaguiOS se elige segun el equipo, ajustes.h) */
void draw_set_view_distance(int distance);
int draw_get_view_distance(void);
const char *draw_renderer(void);   /* glGetString(GL_RENDERER), tras draw_init */
void ui_pointer(int window_w, int window_h, int x, int y, float *ux, float *uy);

/* Interface reference coordinates. Rescaled to fit a window at any aspect ratio. */
#define UI_W 960
#define UI_H 540
#define MENU_X 45
#define MENU_Y 356
#define MENU_STEP 43
#define MENU_BUTTON_W 329
#define SEED_Y 221
#define SIZE_Y 283
#define SIZE_STEP 112
#define SIZE_WIDTH 105
#define FIELD_HEIGHT 32
#define BAG_X 48
#define BAG_Y 140
#define BAG_STEP 46
#define BAG_COLUMNS 8
#define RECIPE_X 450
#define RECIPE_Y 143
#define RECIPE_STEP 27
#define RECIPE_ROWS 12
/* Primera receta visible: la lista se desplaza para mantener el cursor a la vista. */
static inline int recipe_scroll(int cursor,int count)
{
    int first=cursor-RECIPE_ROWS/2;
    if (first>count-RECIPE_ROWS) first=count-RECIPE_ROWS;
    return first<0 ? 0 : first;
}

#endif
