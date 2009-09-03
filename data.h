typedef const struct {
    uint8_t x;
    uint8_t y;
    uint8_t data[0x90];
} sprite_t;

extern sprite_t sprites[];

extern const uint8_t *const levels[8];

enum {
    SPRITE_NULL,
    SPRITE_WALL,
    SPRITE_LADDER,
    SPRITE_EGG,
    SPRITE_GRAIN,
    SPRITE_LIFT,

    SPRITE_PLAYER_R, /* 6 */
    SPRITE_PLAYER_R2,
    SPRITE_PLAYER_R3,
    SPRITE_PLAYER_L,
    SPRITE_PLAYER_L2,
    SPRITE_PLAYER_L3,
    SPRITE_PLAYER_UP, /* 0x0c */
    SPRITE_PLAYER_UP2,
    SPRITE_PLAYER_UP3,

    SPRITE_BIGDUCK_R1, /* 0x0f */
    SPRITE_BIGDUCK_R2,
    SPRITE_BIGDUCK_L1,
    SPRITE_BIGDUCK_L2,

    SPRITE_CAGE_CLOSED, /* 0x13 */
    SPRITE_CAGE_OPEN,

    SPRITE_DUCK_R, /* 0x15 */
    SPRITE_DUCK_R2,
    SPRITE_DUCK_L,
    SPRITE_DUCK_L2,
    SPRITE_DUCK_UP,
    SPRITE_DUCK_UP2,
    SPRITE_DUCK_EAT_R,
    SPRITE_DUCK_EAT_R2,
    SPRITE_DUCK_EAT_L,
    SPRITE_DUCK_EAT_L2,

    SPRITE_0, /* 0x1f */
    SPRITE_1,
    SPRITE_2,
    SPRITE_3,
    SPRITE_4,
    SPRITE_5,
    SPRITE_6,
    SPRITE_7,
    SPRITE_8,
    SPRITE_9,

    SPRITE_SCORE, /* 0x29 */
    SPRITE_BLANK,
    SPRITE_PLAYER,
    SPRITE_LEVEL,
    SPRITE_BONUS,
    SPRITE_TIME,
    SPRITE_HAT /* 0x2f */
};
