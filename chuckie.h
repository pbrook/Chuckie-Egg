#ifndef CHUCKIE_E
#define CHUCKIE_H

#include "config.h"

#include <stdint.h>

/*** Game interface.  */

#define BUTTON_RIGHT  1
#define BUTTON_LEFT   2
#define BUTTON_DOWN   4
#define BUTTON_UP     8
#define BUTTON_JUMP   0x10
extern uint8_t buttons;
extern uint8_t button_ack;
extern int cheat;
extern int skip_frame;

void PollKeys(void);
void RenderFrame(void);
void RenderBackground(void);

void run_game(void);

/*** Audio interface */

#define BUFSIZE 512
#define FREQ 44100
extern short next_buffer[BUFSIZE];
void mix_buffer(void);
void sound_start(int, int);
void init_sound(void);
void init_input(void);

/*** Misc.  */
extern const uint8_t *const levels[8];

/*** Data for rendering engine.  */

#define TILE_WALL     1
#define TILE_LADDER   2
#define TILE_EGG      4
#define TILE_GRAIN    8

/* Direction bitflags  */
#define DIR_L		1
#define DIR_R		2
#define DIR_UP		4
#define DIR_DOWN	8
#define DIR_HORIZ	(DIR_R | DIR_L)

typedef enum {
    PLAYER_WALK,
    PLAYER_CLIMB,
    PLAYER_JUMP,
    PLAYER_FALL,
    PLAYER_LIFT
} player_mode_t;

extern int current_player;
extern int num_players;
extern int current_level;

extern int player_x;
extern int player_y;
extern player_mode_t player_mode;
extern int player_face;
extern int move_x;
extern int move_y;
extern uint8_t bonus[4];
extern uint8_t timer_ticks[3];
extern uint8_t levelmap[20 * 25];
extern int have_lift;
extern int lift_x;
extern int lift_y[2];

extern int have_big_duck;
extern int big_duck_frame;
extern int big_duck_x;
extern int big_duck_y;
extern int big_duck_dir;

typedef enum {
    DUCK_BORED,
    DUCK_STEP,
    DUCK_EAT1,
    DUCK_EAT2,
    DUCK_EAT3,
    DUCK_EAT4
} duck_state;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t tile_x;
    uint8_t tile_y;
    duck_state mode;
    int dir;
} duckinfo_t;
extern duckinfo_t duck[5];
extern int num_ducks;

typedef struct {
    uint8_t score[8];
    uint8_t bonus[4];
    uint8_t egg[16];
    uint8_t grain[16];
    int lives;
} playerdata_t;

extern playerdata_t all_player_data[4];

int DuckSprite(int n);
#endif
