typedef const struct {
    uint8_t x;
    uint8_t y;
    uint8_t data[0x90];
} sprite_t;

extern sprite_t sprites[];

extern const uint8_t *const levels[8];

extern sprite_t SPRITE_NULL;
extern sprite_t SPRITE_WALL;
extern sprite_t SPRITE_LADDER;
extern sprite_t SPRITE_EGG;
extern sprite_t SPRITE_GRAIN;
extern sprite_t SPRITE_LIFT;

extern sprite_t SPRITE_PLAYER_R; /* 6 */
extern sprite_t SPRITE_PLAYER_R2;
extern sprite_t SPRITE_PLAYER_R3;
extern sprite_t SPRITE_PLAYER_L;
extern sprite_t SPRITE_PLAYER_L2;
extern sprite_t SPRITE_PLAYER_L3;
extern sprite_t SPRITE_PLAYER_UP; /* 0x0c */
extern sprite_t SPRITE_PLAYER_UP2;
extern sprite_t SPRITE_PLAYER_UP3;

extern sprite_t SPRITE_BIGDUCK_R1; /* 0x0f */
extern sprite_t SPRITE_BIGDUCK_R2;
extern sprite_t SPRITE_BIGDUCK_L1;
extern sprite_t SPRITE_BIGDUCK_L2;

extern sprite_t SPRITE_CAGE_CLOSED; /* 0x13 */
extern sprite_t SPRITE_CAGE_OPEN;

extern sprite_t SPRITE_DUCK_R; /* 0x15 */
extern sprite_t SPRITE_DUCK_R2;
extern sprite_t SPRITE_DUCK_L;
extern sprite_t SPRITE_DUCK_L2;
extern sprite_t SPRITE_DUCK_UP;
extern sprite_t SPRITE_DUCK_UP2;
extern sprite_t SPRITE_DUCK_EAT_R;
extern sprite_t SPRITE_DUCK_EAT_R2;
extern sprite_t SPRITE_DUCK_EAT_L;
extern sprite_t SPRITE_DUCK_EAT_L2;

extern sprite_t sprite_digit[10]; /* 0x1f */

extern sprite_t SPRITE_SCORE; /* 0x29 */
extern sprite_t SPRITE_BLANK;
extern sprite_t SPRITE_PLAYER;
extern sprite_t SPRITE_LEVEL;
extern sprite_t SPRITE_BONUS;
extern sprite_t SPRITE_TIME;
extern sprite_t SPRITE_HAT; /* 0x2f */

extern sprite_t *const sprite_big_duck[4];
extern sprite_t *const sprite_duck[10];

extern sprite_t *const sprite_player_r[4];
extern sprite_t *const sprite_player_l[4];
extern sprite_t *const sprite_player_up[4];
