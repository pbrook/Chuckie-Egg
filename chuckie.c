#include "config.h"

#include <stdint.h>
#include <stdarg.h>

#include "SDL.h"

#define NUM_PLAYERS 1

int is_dead;
int zero_bonus;
int extra_life;
int num_players;
int active_players;
int current_player;
int current_level;
int eggs_left;
int bonus_hold;
int have_lift;
uint8_t lift_x;
uint8_t lift_y1;
uint8_t lift_y2;
uint8_t current_lift;
int have_big_duck;
int duck_timer;
int big_duck_sprite;
uint8_t big_duck_x;
int big_duck_dx;
uint8_t big_duck_y;
int big_duck_dy;
int num_ducks;
int current_duck;
int duck_speed;
uint8_t bonus[4];
uint8_t timer_ticks[3];
uint8_t lives[4];
uint8_t level[4];
uint8_t levelmap[0x200];
uint8_t player_sprite;
uint8_t player_mode; /*0 = Walking, 1 = Climbing, 2 = Jumping, 3 = Falling */
uint8_t player_fall;
uint8_t player_slide;
uint8_t player_face;
uint8_t player_x;
uint8_t player_y;
uint8_t player_tilex;
uint8_t player_tiley;
uint8_t player_partial_x;
uint8_t player_partial_y;
uint8_t move_x;
uint8_t move_y;
uint8_t buttons;
uint32_t rand_high;
uint8_t rand_low;

typedef struct {
    uint8_t score[8];
    uint8_t bonus[4];
    uint8_t egg[16];
    uint8_t grain[16];
} playerdata_t;

playerdata_t all_player_data[4];
#define player_data (&all_player_data[current_player])

struct {
    uint8_t x;
    uint8_t y;
    uint8_t tile_x;
    uint8_t tile_y;
    uint8_t mode;
    uint8_t sprite;
    uint8_t dir;
} duck[5];

#include "ram.c"
#define LD2(addr) (RAM[addr] | (RAM[(addr) + 1] << 8))

SDL_Surface *sdlscreen;

void die(const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  fprintf(stderr, "Error: ");
  vfprintf(stderr, msg, va);
  va_end(va);
  exit(1);
}

uint8_t pixels[160 * 256];

static void Do_RenderSprite(int x, int y, int sprite, int color)
{
    uint8_t *src;
    uint8_t srcbits;
    int bits;
    int sx;
    int sy;
    int i;
    uint8_t *dest;

    y = y ^ 0xff;
    src = &RAM[0x1100 + (sprite << 2)];
    sx = src[0];
    sy = src[1];
    src = &RAM[src[2] + (src[3] << 8)];
    dest = &pixels[x + y * 160];

    bits = 0;

    while (sy--) {
	for (i = 0; i < sx; i++) {
	    if (bits == 0) {
		srcbits = *(src++);
		bits = 8;
	    }
	    if (srcbits & 0x80) {
		*dest ^= color;
	    }
	    srcbits <<= 1;
	    bits--;
	    dest++;
	}
	dest += 160 - sx;
    }
}

static void Do_RenderDigit(int x, int y, int n)
{
    Do_RenderSprite(x, y, n + 0x1f, 2);
}

static void DrawLives(int player)
{
  int x = 0x1b + 0x22 * player;
  int i;

  for (i = 0; i < 6; i++) {
      Do_RenderDigit(x + 1 + i * 5, 0xf7, all_player_data[player].score[i + 2]);
  }

  i = lives[player];
  if (i > 8)
    i = 8;
  while (i--) {
    Do_RenderSprite(x, 0xee, 0x2f, 4);
    x += 4;
  }
}


/* DrawStatics? */
static void do_1cc3(void)
{
  int tmp;

  Do_RenderSprite(0, 0xf8, 0x29, 2); /* 0x1cd3 */
  tmp = current_player * 0x22 + 0x1b;
  Do_RenderSprite(tmp, 0xf8, 0x2a, 2); /* 0x1cee */
  for (tmp = 0; tmp < num_players; tmp++) {
      DrawLives(tmp);
  }
  Do_RenderSprite(0, 0xe8, 0x2b, 2); /* 0x1d10 */
  Do_RenderSprite(0x1b, 0xe7, current_player + 0x20, 2); /* 0x1d22 */
  Do_RenderSprite(0x24, 0xe8, 0x2c, 2); /* 0x1d31 */

  tmp = current_level + 1;
  Do_RenderDigit(0x45, 0xe7, tmp % 10);
  tmp /= 10;
  Do_RenderDigit(0x40, 0xe7, tmp % 10);
  if (tmp > 10)
    Do_RenderDigit(0x3b, 0xe7, tmp / 10);

  Do_RenderSprite(0x4e, 0xe8, 0x2d, 2); /* 0x1d8b */

  Do_RenderDigit(0x66, 0xe7, bonus[0]); /* 0x1d94 */
  Do_RenderDigit(0x6b, 0xe7, bonus[1]);
  Do_RenderDigit(0x70, 0xe7, bonus[2]);
  Do_RenderDigit(0x75, 0xe7, 0); /* 0x1daf */
  Do_RenderSprite(0x7e, 0xe8, 0x2e, 2); /* 0x1dbe */

  tmp = current_level >> 4;
  if (tmp > 8) tmp = 8;
  tmp ^= 0xff;
  tmp = (tmp + 10) & 0xff;
  timer_ticks[0] = tmp;
  Do_RenderDigit(0x91, 0xe7, tmp); /* 0x1dd5 */
  timer_ticks[1] = 0;
  Do_RenderDigit(0x96, 0xe7, 0); /* 0x1de0 */
  timer_ticks[2] = 0;
  Do_RenderDigit(0x9b, 0xe7, 0); /* 0x1deb */
}

/* 0x23c8 */
/* Duck code generates out of bounds reads.  */
static int Do_ReadMap(uint8_t x, uint8_t y)
{
  if (y >= 0x19 || x >= 0x14) {
      return 0;
  }
  return levelmap[x + y * 20];
}

/* 0x23e0 and 0x1a0c */
static void Do_InitTile(int x, int y, int sprite, int n, int color)
{
    levelmap[y * 20 + x] = n;
    Do_RenderSprite(x << 3, (y << 3) | 7, sprite, color);
}

static void LoadLevel(void)
{
    int addr;
    int i;
    int tmp;
    int x;
    int y;
    int num_walls;
    int num_ladders;
    int num_grain;

    do_1cc3();

    addr = LD2(0x0cc0 + ((current_level & 7) << 1));
    num_walls = RAM[addr++];
    num_ladders = RAM[addr++];
    have_lift = RAM[addr++];
    num_grain = RAM[addr++];
    num_ducks = RAM[addr++];
    /* 0x1b64 */
    for (i = 0; i < 0x200; i++)
      levelmap[i] = 0;

    /* 0x1b70 */
    while (num_walls--) {
	y = RAM[addr++];
	x = RAM[addr++];
	i = RAM[addr++] - x;

label_1b90:
	Do_InitTile(x, y, 1, 1, 3); /* 0x1b96 */
	x++;
	i--;
	if (i >= 0) /* 0x1ba4 */
	  goto label_1b90;
    }

    while (num_ladders--) {
	x = RAM[addr++];
	y = RAM[addr++];
	i = RAM[addr++] - y;

label_1bc6:
	tmp = levelmap[x + y * 20];
	if (tmp) { /* 0x1bcd */
	  Do_RenderSprite(x << 3, (y << 3) | 7, tmp, 3);
	}
	Do_InitTile(x, y, 2, tmp | 2, 2); /* 0x1be4 */
	y++;
	i--;
	if (i >= 0) /* 0x1bf4 */
	  goto label_1bc6;
    }

    if (have_lift) { /* 0x1bfc */
	int tmp;
	tmp = RAM[addr++];
	tmp <<= 3;
	lift_x = tmp;
    }

    /* 0c1ca0 */
    eggs_left = 0;
    for (i = 0; i < 0xc; i++) {
	x = RAM[addr++];
	y = RAM[addr++];
	if (player_data->egg[i] == 0) {
	    Do_InitTile(x, y, 3, (i << 4) | 4, 1);
	    eggs_left++;
	}
    }

    for (i = 0; i < num_grain; i++) {
	x = RAM[addr++];
	y = RAM[addr++];
	if (player_data->grain[i] == 0) {
	    Do_InitTile(x, y, 4, (i << 4) | 8, 2);
	}
    }

    /* 0x1c94 */
    Do_RenderSprite(0, 0xdc, have_big_duck ? 0x14 : 0x13, 4); /* 0x1caa */

    for (i = 0; i < 5 ; i++) {
	duck[i].tile_x = RAM[addr++];
	duck[i].tile_y = RAM[addr++];
    }
}

/* DrawBigDuck */
static void DrawBigDuck(void)
{
  Do_RenderSprite(big_duck_x, big_duck_y, big_duck_sprite + 0x0f, 4);
}

/* DrawDuck */
static void DrawDuck(int n)
{
  int sprite;
  int x;

  sprite = duck[n].sprite + 0x15;
  x = duck[n].x;
  if (sprite >= 0x1d)
    x -= 8;
  Do_RenderSprite(x, duck[n].y, sprite, 8);
}

/* DrawBigBird? */
static void DrawPlayer(void)
{
  Do_RenderSprite(player_x, player_y, player_sprite, 4);
}

static void DrawLastLife(void)
{
  int tmp;
  tmp = lives[current_player];
  if (tmp >= 9) /* 0x2f64 */
      return;
  tmp = (tmp << 2) + (current_player * 0x22) + 0xd + 0xa;
  Do_RenderSprite(tmp, 0xee, 0x2f, 4);
}

static void StartLevel(void)
{
  int i;
  if (have_lift) { /* 0x2e94 */
      lift_y1 = 8;
      lift_y2 = 0x5a;
      current_lift = 0;
      Do_RenderSprite(lift_x, lift_y1, 5, 1); /* 0x2eb2 */
      Do_RenderSprite(lift_x, lift_y2, 5, 1); /* 0x2ec1 */
  }
  big_duck_x = 4;
  big_duck_y = 0xcc;
  big_duck_dx = big_duck_dy = 0;
  big_duck_sprite = 0;
  DrawBigDuck();
  if ((current_level >> 3) == 1) { /* 0x2edf */
      num_ducks = 0;
  }
  if (current_level >= 24) { /* 0x2ee7 */
      num_ducks = 5;
  }
  i = -1;
label_2eed:
  i++;
  if (i < num_ducks) { /* 0x2ef3 */
      duck[i].x = duck[i].tile_x << 3;
      duck[i].y = (duck[i].tile_y << 3) + 0x14;
      duck[i].mode = 0;
      duck[i].sprite = 0;
      duck[i].dir = 2;
      DrawDuck(i);
      goto label_2eed; /* 0x2f1a */
  }
  /* 0x2f1d */
  /* Delay(3) */
  player_x = 0x3c;
  player_y = 0x20;
  player_sprite = 6;
  DrawPlayer();
  player_tilex = 7;
  player_tiley = 2;
  player_partial_x = 7;
  player_partial_y = 0;
  player_mode = 0;
  player_face = 1;
  DrawLastLife(); /* 0x2f45 */
}

/* MoveSideways */
static int do_2276(void)
{
  int tmp, x, y;
  tmp = move_x;
  if ((tmp & 0x80) != 0) /* 0x2278 */
    goto label_227e;
  if (tmp != 0)
    goto label_22bd;
  return 0;
label_227e:
  if (player_x == 0) /* 0x2282 */
    goto label_22fc;
  if (player_partial_x >= 2) /* 0x2288 */
    goto label_22fa;
  if (move_y == 2) /* 0x228e */
    goto label_22fa;
  x = player_tilex - 1;
  y = player_tiley;
  tmp = (uint8_t)(player_partial_y - move_y);
  if (tmp < 8) /* 0x229c */
    goto label_22a5;
  if (((tmp - 8) & 0x80) == 0)
    goto label_22a4;
  y--;
  goto label_22a5; /* 0x22a1 */
label_22a4:
  y++;
label_22a5:
  tmp = Do_ReadMap(x, y); /* 0x22a5 */
  if (tmp == 1) /* 0x22aa */
    goto label_22fc;
  if ((move_y & 0x80) == 0) /* 0x22ae */
    goto label_22fa;
  x = player_tilex - 1;
  y++;
  tmp = Do_ReadMap(x, y); /* 0x22b4 */
  if (tmp == 1)
    goto label_22fc;
  else
    goto label_22fa;
label_22bd:
  tmp = player_x;
  if (tmp >= 0x98) /* 0x22c1 */
    goto label_22fc;
  tmp = player_partial_x;
  if (tmp < 5) /* 0x22c7 */
    goto label_22fa;
  if (move_y == 2) /* 0x22cd */
    goto label_22fa;
  x = player_tilex + 1;
  y = player_tiley;
  tmp = (uint8_t)(player_partial_y + move_y);
  if (tmp < 8) /* 0x22db */
    goto label_22e4;
  if (((tmp - 8) & 0x80) == 0) /* 0x22dd */
    goto label_22e3;
  y--;
  goto label_22e4; /* 0x22e0 */
label_22e3:
  y++;
label_22e4:
  tmp = Do_ReadMap(x, y);
  if (tmp == 1)
    goto label_22fc;
  if ((move_y & 0x80) == 0) /* 0x22ed */
    goto label_22fa;
  x = player_tilex + 1;
  y++;
  tmp = Do_ReadMap(x, y);
  if (tmp == 1)
    goto label_22fc;
label_22fa:
  return 0;
label_22fc:
  return 1;
}

static void do_22fe(int x, int y)
{
  Do_InitTile(x, y, 3, 0, 1);
}

static void do_2311(int x, int y)
{
  Do_InitTile(x, y, 4, 0, 2);
}

static void DrawScoreChange(int n, int oldval, int newval)
{
    int x;
    if (n < 2) /* 0x1ada */
	return;
    x = (current_player * 0x22) + 0x12 + n * 5;;
    Do_RenderDigit(x, 0xf7, oldval); /* 0x1af2 */
    Do_RenderDigit(x, 0xf7, newval); /* 0x1afb */
}

/* AddScore */
static void do_1ab5(int x, int a)
{
  int tmp;
  int y;
  tmp = a;
label_1ab5:
  y = player_data->score[x];
  tmp += y;
  if (x == 3)
    extra_life++;
  if (tmp < 0x0a) /* 0x1ac6 */
    goto label_1ad6;
  tmp -= 0x0a;
  player_data->score[x] = tmp;
  DrawScoreChange(x, y, tmp); /* 0x1acd */
  tmp = 1;
  x--;
  if ((x & 0x80) == 0) /* 0x1ad3 */
    goto label_1ab5;
  return;
label_1ad6:
  player_data->score[x] = tmp;
  DrawScoreChange(x, y, tmp);
}

static void MovePlayer(void)
{
  int x, y, tmp;
  int tmp2;
  int y1, y2;

  move_x = 0;
  move_y = 0;
  if (buttons & 0x01) { /* 0x1e6c */
      move_x++;
  }
  if (buttons & 0x02) { /* 0x1e71 */
      move_x--;
  }
  if (buttons & 0x04) { /* 0x1e76 */
      move_y--;
  }
  if (buttons & 0x08) { /* 0x1e7b */
      move_y++;
  }
  move_y <<= 1;
  /* 0x1e81 */
  tmp = player_mode;
  if (tmp == 2) {
      /* Jump */
      /* 0x1f81 */
label_1f81:
      move_x = player_slide;
      tmp2 = move_y;
      tmp = player_fall >> 2;
      if (tmp >= 6) /* 0x1f8f */
	tmp = 6;
      tmp ^= 0xff;
      tmp += 3;
      move_y = tmp;
      player_fall++;
      if (player_y == 0xdc) { /* 0x1fa0 */
	  move_y = 0xff;
	  player_fall = 0x0c;
	  goto label_2062;
      }
      /* 0x1fad */
      tmp = (int8_t)(player_partial_x + move_x);
      if (tmp != 3) /* 0x1fb4 */
	  goto label_2016;
      if (tmp2 == 0) /* 0x1fb8 */
	goto label_2016;
      if ((tmp2 & 0x80) != 0) /* 0x1fba */
	goto label_1fed;
      /* 0x1fbc */
      x = player_tilex;
      y = player_tiley + 1;
      tmp = Do_ReadMap(x, y); /* 0x1fc1 */
      if ((tmp & 2) != 0) /* 0x1fc6 */
	  goto label_1fdb;
      if (player_partial_y >= 4) /* 0x1fd1 */
	y++;
      tmp = Do_ReadMap(x, y); /* 0x1fd4 */
      if ((tmp & 2) == 0) /* 0x1fd7 */
	goto label_2016;
label_1fdb:
      player_mode = 1;
      tmp = player_partial_y + move_y;
      if (tmp & 1) /* 0x1fe6 */
	  move_y++;
      goto label_20cd; /* 0x1fea */
label_1fed:
      x = player_tilex;
      y = player_tiley;
      tmp = Do_ReadMap(x, y); /* 0x1ff1 */
      if ((tmp & 2) == 0) /* 0x1ff6 */
	goto label_2016;
      x = player_tilex;
      y = player_tiley + 1;
      tmp = Do_ReadMap(x, y); /* 0x1ffd */
      if ((tmp & 2) == 0) /* 0x2002 */
	goto label_2016;
      player_mode = 1;
      tmp = player_partial_y + move_y;
      if (tmp & 1) /* 0x1fe6 */
	  move_y--;
      goto label_20cd;
label_2016:
      tmp = (uint8_t)(move_y + player_partial_y);
      if (tmp == 0)
	goto label_2039;
      if ((tmp & 0x80) == 0) {
	  /* 0x204c */
	  if (tmp != 8)
	    goto label_2062;
	  x = player_tilex;
	  y = player_tiley;
	  tmp = Do_ReadMap(x, y); /* 0x2054 */
	  if ((tmp & 1) == 0) /* 0x2059 */
	    goto label_2062;
	  player_mode = 0;
	  goto label_2062;
      }
      /* 0x201f */
      x = player_tilex;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y); /* 0x2024 */
      if ((tmp & 1) == 0) /* 0x2029 */
	goto label_2062;
      player_mode = 0;
      move_y = -player_partial_y;
      goto label_2062; /* 0x2036 */
label_2039:
      x = player_tilex;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y); /* 0x203e */
      if ((tmp & 1) == 0) /* 0x2041 */
	goto label_2062;
      player_mode = 0;
      goto label_2062;
label_2062:
      if (!have_lift)
	goto label_20bf;
      tmp = (uint8_t)(lift_x - 1);
      if (tmp >= player_x) /* 0x206d */
	goto label_20bf;
      tmp = (uint8_t)(tmp + 0x0a);
      if (tmp < player_x) /* 0x2073 */
	goto label_20bf;
      /* 0x2075 */
      y1 = player_y - 0x11;
      y2 = player_y - 0x13 + move_y;
      tmp = lift_y1;
      if (tmp == y1) /* 0x2087 */
	goto label_208f;
      if (tmp >= y1) /* 0x2089 */
	goto label_2099;
      if (tmp < y2) /* 0x208d */
	goto label_2099;
label_208f:
      if (current_lift != 0) /* 0x20a7 */
	tmp++;
      goto label_20ac; /* 0x2096 */
label_2099:
      tmp = lift_y2;
      if (tmp == y1) /* 0x209d */
	goto label_20a5;
      if (tmp >= y1)
	goto label_20bf;
      if (tmp < y2) /* 0x20a3 */
	goto label_20bf;
label_20a5:
      if (current_lift == 0) /* 0x20a7 */
	tmp++;
label_20ac:
      tmp -= y1;
      move_y = tmp + 1;
      player_fall = 0;
      player_mode = 4;
      goto label_20cd; /* 0x20bc */
label_20bf:
      if (do_2276()) {
	  move_x = -move_x;
	  player_slide = move_x;
      }
label_20cd:
      goto label_217b; /* 0x20cd */
  } else if (tmp == 3) {
      /* Fall */
      /* 0x20e3 */
      player_fall++;
      tmp = player_fall;
      if (tmp < 4) { /* 0x20e9 */
	  move_x = player_slide;
	  move_y = 0xff;
	  goto label_2108; /* 0x20f3 */
      }
      /* 0x20f6 */
      move_x = 0;
      tmp = player_fall >> 2;
      if (tmp >= 4) /* 0x2100 */
	tmp = 3;
      move_y = ~tmp;
label_2108:
      tmp = (int8_t)(move_y + player_partial_y);
      if (tmp == 0) /* 0x210d */
	goto label_212b;
      if (tmp >= 0) /* 0x210f */
	goto label_213b;
      x = player_tilex;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y); /* 0x2116 */
      if ((tmp & 1) == 0) /* 0x211b */
	goto label_213b;
      player_mode = 0;
      move_y = -player_partial_y;
      goto label_213b; /* 0x2128 */
label_212b:
      x = player_tilex;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y); /* 0x2130 */
      if ((tmp & 1) != 0) /* 0x2135 */
	player_mode = 0;
label_213b:
      goto label_217b;
  } else if (tmp == 1) {
      /* Climb */
      /* 0x1f22 */
      if ((buttons & 0x10) != 0) /* 0x1f26 */
	goto label_20d0;
      tmp = move_x;
      if (tmp == 0) /* 0x1f2d */
	goto label_1f4a;
      x = player_partial_y;
      if (x != 0) /* 0x1f31 */
	goto label_1f4a;
      x = player_tilex;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y); /* 0x1f38 */
      if ((tmp & 1) == 0) /* 0x1f3d */
	goto label_1f4a;
      move_y = 0;
      player_mode = 0;
      goto label_1f7a; /* 0x1f47 */
label_1f4a:
      move_x = 0;
      if (move_y == 0) /* 0x1f50 */
	goto label_1f7a;
      if (player_partial_y != 0) /* 0x1f54 */
	goto label_1f7a;
      if ((move_y & 0x80) != 0) /* 0x1f56 */
	goto label_1f6c;
      x = player_tilex;
      y = player_tiley + 2;
      tmp = Do_ReadMap(x, y); /* 0x1f60 */
      if ((tmp & 2) == 0) /* 0x1f65 */
	move_y = 0;
      goto label_1f7a; /* 0x1f69 */
label_1f6c:
      x = player_tilex;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y); /* 0x1f71 */
      if ((tmp & 2) == 0) /* 0x1f76 */
	move_y = 0;
label_1f7a:
      player_face = 0;
      goto label_217b;
  } else if (tmp != 0) {
      /* 0x213e */
      if ((buttons & 0x10) != 0) /* 0x2142 */
	goto label_20d0;
      tmp = (uint8_t)(lift_x - 1);
      if (tmp >= player_x) /* 0x214e */
	goto label_2156;
      tmp = (uint8_t)(tmp + 0x0a);
      if (tmp >= player_x) /* 0x2154 */
	goto label_2160;
label_2156:
      player_fall = 0;
      player_slide = 0;
      player_mode = 3;
label_2160:
      move_y = 1;
      tmp= move_x;
      if (tmp != 0) /* 0x2166 */
	player_face = tmp;
      if (do_2276()) { /* 0x216a */
	  move_x = 0;
      }
      tmp = player_y;
      if (tmp < 0xdc) /* 0x2177 */
	goto label_217b;
      is_dead++;
label_217b:
      DrawPlayer();
      player_x += move_x;
      tmp = (uint8_t)(player_partial_x + move_x);
      if ((tmp & 0x80) != 0) /* 0x218c */
	player_tilex--;
      if (((tmp - 8) & 0x80) == 0) /* 0x2192 */
	player_tilex++;
      player_partial_x = tmp & 7;
      player_y += move_y;
      tmp = (uint8_t)(player_partial_y + move_y);
      if ((tmp & 0x80) != 0) /* 0x21a6 */
	player_tiley--;
      if (((tmp - 8) & 0x80) == 0) /* 0x21ac */
	player_tiley++;
      player_partial_y = tmp & 7;
      x = 6;
      /* 0x21b6 */
      tmp = player_face;
      if (tmp == 0) /* 0x21b8 */
	goto label_21c6;
      if ((tmp & 0x80) == 0) /* 0x21ba */
	goto label_21be;
      x = 9;
label_21be:
      tmp2 = x;
      tmp = player_partial_x >> 1;
      goto label_21cd; /* 0x21c3 */
label_21c6:
      tmp2 = 0x0c;
      tmp = player_partial_y >> 1;
label_21cd:
      x = 2;
      if (x != 0) { /* 0x21d3 */
	  tmp = (tmp & 1) << 1;
      }
      x = player_mode;
      if (x != 1) /* 0x21dc */
	goto label_21e7;
      x = move_y;
      if (x != 0) /* 0x21e0 */
	goto label_21ed;
      tmp = 0;
      goto label_21ed; /* 0x21e4 */
label_21e7:
      x = move_x;
      if (x != 0) /* 0x21e9 */
	goto label_21ed;
      tmp = 0;
label_21ed:
      tmp += tmp2;
      player_sprite = tmp;
      DrawPlayer();
      x = player_tilex;
      y = player_tiley;
      tmp = player_partial_y;
      if (tmp >= 4) /* 0x21fd */
	y++;
      tmp = Do_ReadMap(x, y); /* 0x2202 */
      if ((tmp & 0x0c) == 0) /* 0x2209 */
	goto label_2275;
      if ((tmp & 0x08) != 0) /* 0x220d */
	goto label_2248;
      /* Got egg */
      eggs_left--;
      /* 0x2211 */
      /* BEEP(6) */
      /* 0x221f */
      tmp >>= 4;
      player_data->egg[tmp]--;
      x = player_tilex;
      do_22fe(x, y); /* 0x2230 */
      tmp = (current_level >> 2) + 1;
      if (tmp >= 0x0a) /* 0x223c */
	tmp = 0x0a;
      x = 5;
      do_1ab5(x, tmp);
      goto label_2275; /* 0x2245 */
label_2248:
      /* Got grain */
      /* BEEP(5) */
      /* 0x2256 */
      tmp >>= 4;
      player_data->grain[tmp]--;
      x = player_tilex;
      do_2311(x, y); /* 0x2267 */
      tmp = 5;
      x = 6;
      do_1ab5(x, tmp); /* 0x226e */
      bonus_hold = 14;
label_2275:
      return; /* 0x2275 */
  } else {
      /* Walk */
      /* 0x1e9b */
      if (buttons & 0x10) { /* 0x1e9f */
	  /* 0x20d0 */
label_20d0:
	  player_fall = 0;
	  player_mode = 2;
	  tmp = move_x;
	  player_slide = tmp;
	  if (tmp != 0) /* 0x20dc */
	    player_face = tmp;
	  goto label_1f81; /* 0x20e0 */
      } else if (move_y) { /* 0x1ea6 */
	  /* 0x1ea8 */
	  if (player_partial_x == 3) { /* 0x1eac */
	      if ((move_y & 0x80) == 0) { /* 0x1eb0 */
		  x = player_tilex;
		  y = player_tiley + 2;
		  tmp = Do_ReadMap(x, y); /* 0x1ebe */
		  if ((tmp & 2) == 0) /* 0x1ebd */
		    goto label_1ed8;
		  goto label_1ecd;

	      }
	      /* 0x1ec1 */
	      x = player_tilex;
	      y = player_tiley - 1;
	      tmp = Do_ReadMap(x, y); /* 0x1ec6 */
	      if ((tmp & 2) == 0) /* 0x1ecb */
		goto label_1ed8;
label_1ecd:
	      move_x = 0;
	      player_mode = 1;
	      goto label_1f19; /* 0x1ed5 */
	  }
	  goto label_1ed8;
      } else {
label_1ed8:
	  /* 0x1ed8 */
	  move_y = 0;
	  tmp = (int8_t)(player_partial_x + move_x);
	  x = player_tilex;
	  if (tmp < 0)
	    x--;
	  else if (tmp >= 8)
	    x++;
	  y = player_tiley - 1;
	  tmp = Do_ReadMap(x, y); /* 0x1ef0 */
	  tmp &= 1;
	  if (tmp == 0) { /* 0x1ef5 */
	      int n;
	      y = tmp;
	      x = 0xff;
	      n = (move_x + player_partial_x) & 7;
	      if (n < 4) { /* 0x1f03 */
		  x = 1;
		  y++;
	      }
	      player_slide = x;
	      player_fall = y;
	      player_mode = 3;
	  }
	  /* 0x1f10 */
	  if (do_2276()) { /* 0x1f13 */
	      move_x = 0;
	  }
label_1f19:
	  if (move_x) { /* 0x1f1b */
	      player_face = move_x;
	  }
	  goto label_217b;
      }
  }
}

static void MakeSound(void)
{
  int tmp;
  if (!(move_x || move_y))
    return;
  if ((duck_timer & 1) != 0)
    return;
  tmp = player_mode;
  if (tmp == 0) { /* 0x0c48 */
      tmp = 64;
      goto label_0c8b;
  }
  if (tmp == 1) { /* 0x0c51 */
      tmp = 96;
      goto label_0c8b;
  }
  if (tmp != 2) /*0x0c5a */
    goto label_0c76;
  tmp = player_fall;
  if (tmp >= 0x0b) { /* 0x0c60 */
      tmp = (uint8_t)(0xbe - (player_fall * 2));
      goto label_0c8b;
  }
  tmp = (uint8_t)(0x96 - (player_fall * 2));
  goto label_0c8b;
label_0c76:
  if (tmp == 3) { /* 0x0c78 */
    tmp = (uint8_t)(0x6e - (player_fall * 2));
    goto label_0c8b;
  }
  if (move_x == 0) /* 0x0c86 */
    return;
  tmp = 0x64;
label_0c8b:
  /* BEEP(tmp) */
  while(0);
}

/* MoveLift */
static void MoveLift(void)
{
  int y;

  if (!have_lift)
    return;
  if (current_lift == 0)
    y = lift_y1;
  else
    y = lift_y2;
  Do_RenderSprite(lift_x, y, 5, 1); /* 0x2392 */
  y += 2;
  if (y == 0xe0)
    y = 6;
  Do_RenderSprite(lift_x, y, 5, 1); /* 0x23af */
  if (current_lift == 0) {
      lift_y1 = y;
  } else {
      lift_y2 = y;
  }
  current_lift = !current_lift;
}

/* popcount */
static int do_25a9(int tmp)
{
  int x;

  x = 0;
label_25ad:
  if ((tmp & 1) != 0) /* 0x25ae */
    x++;
  tmp >>= 1;
  if (tmp != 0) /* 0x25b3 */
    goto label_25ad;
  return x;
}

static void do_1aa4(void)
{
  int carry;

  carry = (((rand_low & 0x48) + 0x38) & 0x40) != 0;
  rand_high = (rand_high << 1) | carry;
  rand_low = (rand_low << 1) | ((rand_high >> 24) & 1);
}

static void DrawTimer(int n)
{
  Do_RenderDigit(0x91 + n * 5, 0xe7, timer_ticks[n]);
}

static void DrawBonus(int n)
{
  Do_RenderDigit(0x66 + n * 5, 0xe7, bonus[n]);
}

static void ReduceBonus(void)
{
  int n;
  int flag;

  n = 2;
label_26e0:
  DrawBonus(n);
  bonus[n]--;
  flag = ((bonus[n] & 0x80) != 0);
  if (flag) { /* 0x26e8 */
      bonus[n] = 9;
  }
  DrawBonus(n);
  n--;
  if (flag)
    goto label_26e0;
  if (bonus[0] + bonus[1] + bonus[2] == 0)
    zero_bonus = 1;
}

static void MoveDucks(void)
{
  int tmp;
  int y;
  int x;
  int flag;
  int duck_look;
  int tmp2;
  int newdir;

  /* Big Duck.  */
  duck_timer++;
  if (duck_timer == 8) {
      duck_timer = 0;
      goto label_2420;
  }
  if (duck_timer == 4) /* 0x2418 */
    goto label_269d;
  goto label_24b5;
label_2420:
  duck_look = big_duck_sprite & 2;
  if (!have_big_duck) /* 0x2428 */
    goto label_2494;
  tmp = (uint8_t)(big_duck_x + 4);
  if (tmp >= player_x) /* 0x2431 */
    goto label_2444;
  if (big_duck_dx < 5)
    big_duck_dx++;
  duck_look = 0;
  goto label_2452; /* 0x2441 */
label_2444:
  if (big_duck_dx > -5)
    big_duck_dx--;
  duck_look = 2;
label_2452:
  tmp = player_y + 4;
  if (tmp < big_duck_y) /* 0x2459 */
    goto label_2468;
  if (big_duck_dy < 5)
    big_duck_dy++;
  goto label_2472;
label_2468:
  if (big_duck_dy > -5)
    big_duck_dy--;
label_2472:
  tmp = (uint8_t)(big_duck_y + big_duck_dy);
  if (tmp >= 0x28) /* 0x2479 */
    goto label_2483;
  big_duck_dy = -big_duck_dy;
label_2483:
  tmp = (uint8_t)(big_duck_x + big_duck_dx);
  if (tmp < 0x90) /* 0x248a */
    goto label_2494;
  big_duck_dx = -big_duck_dx;
label_2494:
  DrawBigDuck();
  big_duck_x += big_duck_dx;
  big_duck_y += big_duck_dy;
  big_duck_sprite = (~big_duck_sprite & 1) | duck_look;
  DrawBigDuck();
  return;
label_24b5:
  if (current_duck == 0)
    current_duck = duck_speed;
  else
    current_duck--;
  if (current_duck >= num_ducks) /* 0x24c1 */
    return;
  /* Move little duck.  */
  tmp = duck[current_duck].mode;
  if (tmp == 1) /* 0x24cb */
    goto label_25ef;
  if (tmp >= 1) /* 0x24d0 */
    goto label_25b6;
  x = duck[current_duck].tile_x;
  y = duck[current_duck].tile_y;
  newdir = 0;
  tmp = Do_ReadMap(x - 1, y - 1); /* 0x24e9 */
  if ((tmp & 1) != 0)
    newdir = 1;
  tmp = Do_ReadMap(x + 1, y - 1); /* 0x24f8 */
  if ((tmp & 1) != 0)
    newdir |= 2;
  tmp = Do_ReadMap(x, y - 1); /* 0x250a */
  if ((tmp & 2) != 0)
    newdir |= 8;
  tmp = Do_ReadMap(x, y + 2); /* 0x251d */
  if ((tmp & 2) != 0)
    newdir |= 4;
  if (do_25a9(newdir) == 1) { /* 0x252f */
      duck[current_duck].dir = newdir;
      goto label_257b; /* 0x2535 */
  }
  tmp = duck[current_duck].dir;
  if (tmp < 4) { /* 0x2542 */
      tmp ^= 0xfc;
  } else {
      tmp ^= 0xf3;
  }
  newdir &= tmp;
  if (do_25a9(newdir) == 1) { /* 0x2554 */
      duck[current_duck].dir = newdir;
      goto label_257b; /* 0x255d */
  }
  tmp2 = newdir;
label_2564:
  do_1aa4(); /* 0x2564 */
  newdir = rand_low & tmp2;
  if (do_25a9(newdir) != 1) /* 0x2572 */
    goto label_2564;
  duck[current_duck].dir = newdir;
label_257b:
  tmp = duck[current_duck].dir;
  tmp &= 3;
  if (tmp == 0) /* 0x2582 */
    goto label_25ef;
  tmp &= 1;
  if (tmp == 0) /* 0x2586 */
    goto label_2593;
  tmp = Do_ReadMap(x - 1, y); /* 0x258d */
  goto label_259b;
label_2593:
  tmp = Do_ReadMap(x + 1, y); /* 0x2598 */
label_259b:
  tmp &= 8;
  if (tmp == 0)
    goto label_25ef;
  tmp = 2;
  duck[current_duck].mode = tmp;
  goto label_25ef; /* 0x25a6 */
label_25b6:
  if (tmp != 4)
    goto label_25ef;
  x = duck[current_duck].tile_x - 1;
  y = duck[current_duck].tile_y;
  if ((duck[current_duck].dir & 1) == 0) /* 0x25cc */
    x += 2;
  tmp = Do_ReadMap(x, y); /* 0x25d2 */
  if ((tmp & 8) == 0) /* 0x25d9 */
    goto label_25ef;
  player_data->grain[tmp >> 4]--;
  do_2311(x, y); /* 0x25ec */
label_25ef:
  DrawDuck(current_duck);
  tmp = duck[current_duck].mode;
  if (tmp >= 2) /* 0x25f9 */
    goto label_2675;
  tmp = duck[current_duck].dir;
  if ((tmp & 1) != 0) /* 0x25ff */
    goto label_2633;
  if ((tmp & 2) != 0) /* 0x2602 */
    goto label_2649;
  if ((tmp & 4) != 0) /* 0x2605 */
    goto label_261d;
  duck[current_duck].y -= 4;
  tmp = duck[current_duck].mode;
  if (tmp != 0) /* 0x2613 */
    duck[current_duck].tile_y--;
  tmp = 4;
  goto label_265f;
label_261d:
  duck[current_duck].y += 4;
  tmp = duck[current_duck].mode;
  if (tmp != 0) /* 0x2629 */
    duck[current_duck].tile_y++;
  tmp = 4;
  goto label_265f;
label_2633:
  duck[current_duck].x -= 4;
  tmp = duck[current_duck].mode;
  if (tmp != 0) /* 0x263f */
    duck[current_duck].tile_x--;
  tmp = 2;
  goto label_265f;
label_2649:
  duck[current_duck].x += 4;
  tmp = duck[current_duck].mode;
  if (tmp != 0) /* 0x263f */
    duck[current_duck].tile_x++;
  tmp = 0;
  goto label_265f;
label_265f:
  y = duck[current_duck].mode ^ 1;
  duck[current_duck].mode = y;
  tmp += y;
  duck[current_duck].sprite = tmp;
  DrawDuck(current_duck);
  return;
label_2675:
  tmp = duck[current_duck].mode << 1;
  tmp &= 0x1f;
  duck[current_duck].mode = tmp;
  if (tmp != 0)
    tmp = 6;
  y = duck[current_duck].dir;
  if (y == 1) /* 0x2687 */
    tmp += 2;
  y = duck[current_duck].mode;
  if (y == 8) /* 0x2691 */
    tmp++;
  duck[current_duck].sprite = tmp;
  DrawDuck(current_duck);
  return;
label_269d:
  /* Update bonus/timer.  */
  if (bonus_hold) {
      bonus_hold--;
      return;
  }
  x = 2;
label_26ac:
  DrawTimer(x);
  timer_ticks[x]--;
  flag = (timer_ticks[x] & 0x80) != 0;
  if (flag) /* 0x26b4 */
    timer_ticks[x] = 9;
  DrawTimer(x);
  x--;
  if (flag) /* 0x26c0 */
    goto label_26ac;
  tmp = (uint8_t)(timer_ticks[0] + timer_ticks[1] + timer_ticks[2]);
  if (tmp == 0) { /* 0x26c9 */
      is_dead++;
      return; /* 0x26cd */
  }
  tmp = timer_ticks[2];
  if (tmp != 0 && tmp != 5)
    return; /* 0x26d6 */
  if (zero_bonus)
    return;
  ReduceBonus();
}

static void MaybeAddExtraLife(void)
{
  if (extra_life == 0)
    return;
  extra_life = 0;
  DrawLastLife();
  lives[current_player]++;
}

static void CollisionDetect(void)
{
  int x;
  int tmp;
  /* Little ducks */
  if (num_ducks == 0)
    goto label_2758;
  x = 0;
label_2730:
  tmp = (uint8_t)((duck[x].x - player_x) + 5);
  if (tmp >= 0x0b) /* 0x273d */
    goto label_2750;
  tmp = (uint8_t)((duck[x].y - 1) - player_y + 0xe);
  if (tmp >= 0x1d) /* 0x274c */
    goto label_2750;
  is_dead++;
label_2750:
  x++;
  if (x < num_ducks)
    goto label_2730;
label_2758:
  /* Big duck */
  if (!have_big_duck)
    return;
  tmp = (uint8_t)(big_duck_x + 4 - player_x + 5);
  if (tmp >= 0x0b) /* 0x2769 */
    return;
  tmp = (uint8_t)(big_duck_y - 5 - player_y + 0x0e);
  if (tmp >= 0x1d) /* 0x2777 */
    return;
  is_dead++;
}

static void SavePlayerState(void)
{
  int i;
  level[current_player] = current_level;
  for (i = 0; i < 4; i++) /* 0x2e8f */
    player_data->bonus[i] = bonus[i];
}

static void do_2dfe(void)
{
  int a, b;
  int i;
  a = current_level + 1;
  b = current_player << 6;
  if (a >= 10)
    a = 9;
  player_data->bonus[0] = a;
  player_data->bonus[1] = 0;
  player_data->bonus[2] = 0;
  player_data->bonus[3] = 0;
  for (i = 0; i < 16; i++) {
      player_data->egg[i] = 0;
      player_data->grain[i] = 0;
  }
}

static void RestorePlayerState(void)
{
    int y;
    current_level = level[current_player];
    for (y = 0; y < 4; y++) {
	bonus[y] = player_data->bonus[y];
    }
}

static void do_2f7c(int addr)
{
  /* Play tune */
}

/* EndOfFrame */
static int do_29f0(void)
{
  int tmp;
  if (is_dead != 0)
    goto label_2a39;
  if (player_y < 0x11)
    goto label_2a39;
  if (eggs_left == 0) /* 0x29fc */
    goto label_2a05;
  if ((buttons & 0x80) != 0) /* 0x2a00 */
    goto label_29ae;
  /* Next frame */
  return 0x29d8;
label_2a05:
  /* Level complete */
  if (zero_bonus)
    goto label_2a2b;
label_2a09:
  do_1ab5(6, 1);
  ReduceBonus();
  MaybeAddExtraLife();
  tmp = timer_ticks[3];
  if (tmp == 0 || tmp == 5) { /* 0x2a1c */
      /* BEEP(?) */
  }
  /* 0x2a27 */
  if (!zero_bonus)
    goto label_2a09;
label_2a2b:
  /* Advance to next level */
  zero_bonus = 0;
  current_level++;
  SavePlayerState(); /* 0x2a2d */
  do_2dfe();
  RestorePlayerState();
  return 0x29cf;
label_2a39:
  /* Died */
  SavePlayerState();
  do_2f7c(0x2fa6); /* 0x2a40 */
  tmp = --lives[current_player];
  if (tmp != 0)
    goto label_2a70;
  /* Clear Screen */
  /* "Game Over" */
  /* 0x2a64 */
  /* Highscores.  */
  if (--active_players == 0)
    goto label_2a87;
label_2a70:
  /* Select next player. */
  tmp = current_player;
  do {
      tmp = (tmp + 1) & 3;
  } while (tmp >= num_players || lives[tmp] == 0);
  current_player = tmp;
  RestorePlayerState(); /* 0x2a81 */
  return 0x29b4;
label_2a87:
  /* No players left. */
  /* Goto menu */
  goto label_29ae;
label_29ae:
  return 0x29ae;
}


void start_game()
{
  int i, j;
  num_players = active_players = NUM_PLAYERS;
  for (i = 3; i >= 0; i--) {
      current_player = i;
      lives[i] = 5;
      level[i] = 0;
      for (j = 0; j < 8; j++) {
	  player_data->score[j] = 0;
      }
      do_2dfe();
  }
  RestorePlayerState();
}

/* 0x2dc0 */
static void SetupLevel(void)
{
  int arg;

  arg = current_level;
  have_big_duck = (arg > 7);
  duck_timer = 0;
  current_duck = 0;
  duck_speed = (arg < 32) ? 8 : 5;
  extra_life = 0;
  is_dead = 0;
  bonus_hold = 0;
  rand_high = 0x767676;
  rand_low = 0x76;
}

SDLKey keys[8] = {
    SDLK_PERIOD,
    SDLK_COMMA,
    SDLK_z,
    SDLK_a,
    SDLK_SPACE
};
/* Read inputs, and wait until the start of the next frame.  */
static void PollKeys(void)
{
  SDL_Event event;
  int i;

  while (1) {
      if (SDL_WaitEvent(&event) == 0) {
	  die("SDL_WaitEvent: %s\n", SDL_GetError());
      }
      switch (event.type) {
      case SDL_QUIT:
	  SDL_Quit();
	  exit(0);
	  break;
      case SDL_USEREVENT:
	  return;
      case SDL_KEYDOWN:
      case SDL_KEYUP:
	  for (i = 0; i < 8; i++) {
	      if (event.key.keysym.sym == keys[i]) {
		  if (event.key.state == SDL_PRESSED)
		      buttons |= 1 << i;
		  else
		      buttons &= ~(1 << i);
	      }
	  }
	  break;
      }
  }
}

static void ClearScreen(void)
{
  memset(pixels, 0, 160*256);
}

static void RenderFrame(void)
{
  uint8_t *dest;
  uint8_t *src;
  uint8_t color;
  int x;
  int y;

  if (SDL_LockSurface(sdlscreen) > 0)
    die("SDL_LockScreen: %s\n", SDL_GetError());

  dest = sdlscreen->pixels;
  src = pixels;
  for (y = 0; y < 256; y++) {
      for (x = 0; x < 160; x++) {
	  color = *(src++);
	  *(dest++) = color;
	  *(dest++) = color;
      }
      dest += sdlscreen->pitch - 320;
  }
  SDL_UnlockSurface(sdlscreen);
  SDL_UpdateRect(sdlscreen, 0, 0, 0, 0);
}

void run_game()
{
new_game:
  start_game();
next_player:
  /* 0x29b4 -29cc "Get Ready" */
next_level:
  SetupLevel();
  ClearScreen();
  LoadLevel();
  StartLevel();
next_frame:
  PollKeys();
  MovePlayer();
  MakeSound();
  MoveLift();
  MoveDucks();
  MaybeAddExtraLife();
  CollisionDetect();
  RenderFrame();
  switch (do_29f0()) {
  case 0x29d8:
      goto next_frame;
  case 0x29cf:
      goto next_level;
  case 0x29b4:
      goto next_player;
  case 0x29ae: /* Menu */
      goto new_game;
  default:
      abort();
  }
}

#define YELLOW {0xff, 0xff, 0} /* Player, lift, big duck */
#define BLUE {0, 0xff, 0xff} /* Small duck */
#define GREEN {0, 0xff, 0} /* Floor */
#define PURPLE {0xff, 0, 0xff} /* Ladder */

Uint32 do_timer(Uint32 interval, void *param)
{
  SDL_Event event;
  event.type = SDL_USEREVENT;
  SDL_PushEvent(&event);
  return interval;
}

int main()
{
  SDL_Color palette[16] = {
    {0, 0, 0}, YELLOW, PURPLE, GREEN,
    YELLOW, YELLOW, YELLOW, YELLOW, 
    BLUE, BLUE, BLUE, BLUE,
    YELLOW, YELLOW, YELLOW, YELLOW, 
  };

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1) {
      die("SDL_Init: %s\n", SDL_GetError());
  }

  sdlscreen = SDL_SetVideoMode(320, 256, 8, SDL_SWSURFACE);
  SDL_SetPalette(sdlscreen, SDL_LOGPAL | SDL_PHYSPAL, palette, 0, 16);

  SDL_AddTimer(30, do_timer, NULL);

  run_game();
}
