#ifndef _RASTER_H
#define _RASTER_H

typedef struct {
    void (*erase_player)(void);
    void (*draw_player)(void);
    void (*draw_timer)(int n);
    void (*draw_bonus)(int n);
    void (*draw_score)(int n, int oldval, int newval);
    void (*draw_tile)(int x, int y, int type);
    void (*draw_big_duck)(void);
    void (*draw_duck)(int n);
    void (*draw_life)(int n);
    void (*draw_lift)(int x, int y);
    void (*clear_screen)(void);
} raster_hooks;

extern raster_hooks *raster;

extern uint8_t pixels[160 * 256];

#endif /* _RASTER_H */
