/* Chuckie Egg.  Based on the original by A & F Software
   Written by Paul Brook
   Released under teh GNU GPL v3.  */

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chuckie.h"
#include "data.h"

#define NUM_PLAYERS 1

#define COLOR_YELLOW	1
#define COLOR_PURPLE	2
#define COLOR_GREEN	3
#define PLANE_YELLOW	4
#define PLANE_BLUE	8

#define COLOR_HUD	COLOR_PURPLE
#define COLOR_WALL	COLOR_GREEN
#define COLOR_LIFT	COLOR_YELLOW
#define COLOR_EGG	COLOR_YELLOW
#define COLOR_LADDER	COLOR_PURPLE
#define COLOR_GRAIN	COLOR_PURPLE

int cheat;
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
int big_duck_frame;
uint8_t big_duck_x;
int big_duck_dx;
uint8_t big_duck_y;
int big_duck_dy;
int big_duck_dir;
int num_ducks;
int current_duck;
int duck_speed;
uint8_t bonus[4];
uint8_t timer_ticks[3];
uint8_t lives[4];
uint8_t level[4];
uint8_t levelmap[0x200];
sprite_t *player_sprite;
/* 0 = Walking, 1 = Climbing, 2 = Jumping, 3 = Falling, 4 = Lift */
uint8_t player_mode;
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

/* Direction bitflags  */
#define DIR_L		1
#define DIR_R		2
#define DIR_UP		4
#define DIR_DOWN	8
#define DIR_HORIZ	(DIR_R | DIR_L)

typedef struct {
    uint8_t score[8];
    uint8_t bonus[4];
    uint8_t egg[16];
    uint8_t grain[16];
} playerdata_t;

playerdata_t all_player_data[4];
#define player_data (&all_player_data[current_player])

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
    uint8_t dir;
} duckinfo_t;
duckinfo_t duck[5];

uint8_t pixels[160 * 256];

static void Do_RenderSprite(int x, int y, sprite_t *sprite, int color)
{
    const uint8_t *src;
    uint8_t srcbits;
    int bits;
    int sx;
    int sy;
    int i;
    uint8_t *dest;

    y = y ^ 0xff;
    sx = sprite->x;
    sy = sprite->y;
    src = sprite->data;
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
    Do_RenderSprite(x, y, &sprite_digit[n], COLOR_HUD);
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
    Do_RenderSprite(x, 0xee, &SPRITE_HAT, PLANE_YELLOW);
    x += 4;
  }
}


static void DrawHUD(void)
{
  int tmp;

  Do_RenderSprite(0, 0xf8, &SPRITE_SCORE, COLOR_HUD);
  tmp = current_player * 0x22 + 0x1b;
  Do_RenderSprite(tmp, 0xf8, &SPRITE_BLANK, COLOR_HUD);
  for (tmp = 0; tmp < num_players; tmp++) {
      DrawLives(tmp);
  }
  Do_RenderSprite(0, 0xe8, &SPRITE_PLAYER, COLOR_HUD);
  Do_RenderSprite(0x1b, 0xe7, &sprite_digit[current_player + 1], 2);
  Do_RenderSprite(0x24, 0xe8, &SPRITE_LEVEL, COLOR_HUD);

  tmp = current_level + 1;
  Do_RenderDigit(0x45, 0xe7, tmp % 10);
  tmp /= 10;
  Do_RenderDigit(0x40, 0xe7, tmp % 10);
  if (tmp > 10)
    Do_RenderDigit(0x3b, 0xe7, tmp / 10);

  Do_RenderSprite(0x4e, 0xe8, &SPRITE_BONUS, COLOR_HUD);

  Do_RenderDigit(0x66, 0xe7, bonus[0]);
  Do_RenderDigit(0x6b, 0xe7, bonus[1]);
  Do_RenderDigit(0x70, 0xe7, bonus[2]);
  Do_RenderDigit(0x75, 0xe7, 0);
  Do_RenderSprite(0x7e, 0xe8, &SPRITE_TIME, COLOR_HUD);

  tmp = current_level >> 4;
  if (tmp > 8) tmp = 8;
  tmp ^= 0xff;
  tmp = (tmp + 10) & 0xff;
  timer_ticks[0] = tmp;
  Do_RenderDigit(0x91, 0xe7, tmp);
  timer_ticks[1] = 0;
  Do_RenderDigit(0x96, 0xe7, 0);
  timer_ticks[2] = 0;
  Do_RenderDigit(0x9b, 0xe7, 0);
}

/* Duck code generates out of bounds reads.  */
static int Do_ReadMap(uint8_t x, uint8_t y)
{
  if (y >= 0x19 || x >= 0x14) {
      return 0;
  }
  return levelmap[x + y * 20];
}

static void Do_InitTile(int x, int y, sprite_t *sprite, int n, int color)
{
    levelmap[y * 20 + x] = n;
    Do_RenderSprite(x << 3, (y << 3) | 7, sprite, color);
}

static void LoadLevel(void)
{
    int i;
    int tmp;
    int x;
    int y;
    int num_walls;
    int num_ladders;
    int num_grain;
    const uint8_t *p;

    DrawHUD();

    p = levels[current_level & 7];
    num_walls = *(p++);
    num_ladders = *(p++);
    have_lift = *(p++);
    num_grain = *(p++);
    num_ducks = *(p++);
    for (i = 0; i < 0x200; i++)
      levelmap[i] = 0;

    while (num_walls--) {
	y = *(p++);
	x = *(p++);
	i = *(p++);
	while (x <= i) {
	    Do_InitTile(x, y, &SPRITE_WALL, 1, COLOR_WALL);
	    x++;
	}
    }

    while (num_ladders--) {
	x = *(p++);
	y = *(p++);
	i = *(p++);
	while (y <= i) {
	    tmp = levelmap[x + y * 20];
	    if (tmp)
		Do_RenderSprite(x << 3, (y << 3) | 7, &SPRITE_WALL, COLOR_WALL);
	    Do_InitTile(x, y, &SPRITE_LADDER, tmp | 2, COLOR_LADDER);
	    y++;
	}
    }

    if (have_lift) {
	lift_x = *(p++) << 3;
    }

    eggs_left = 0;
    for (i = 0; i < 0xc; i++) {
	x = *(p++);
	y = *(p++);
	if (player_data->egg[i] == 0) {
	    Do_InitTile(x, y, &SPRITE_EGG, (i << 4) | 4, COLOR_EGG);
	    eggs_left++;
	}
    }

    for (i = 0; i < num_grain; i++) {
	x = *(p++);
	y = *(p++);
	if (player_data->grain[i] == 0) {
	    Do_InitTile(x, y, &SPRITE_GRAIN, (i << 4) | 8, COLOR_GRAIN);
	}
    }

    Do_RenderSprite(0, 0xdc, have_big_duck ? &SPRITE_CAGE_OPEN
					   : &SPRITE_CAGE_CLOSED, PLANE_YELLOW);

    for (i = 0; i < 5 ; i++) {
	duck[i].tile_x = *(p++);
	duck[i].tile_y = *(p++);
    }
}

static void DrawBigDuck(void)
{
  sprite_t *sprite;
  if (big_duck_dir) {
      sprite = big_duck_frame ? &SPRITE_BIGDUCK_L2 : &SPRITE_BIGDUCK_L1;
  } else {
      sprite = big_duck_frame ? &SPRITE_BIGDUCK_R2 : &SPRITE_BIGDUCK_R1;
  }
  Do_RenderSprite(big_duck_x, big_duck_y, sprite, PLANE_YELLOW);
}

static void DrawDuck(int n)
{
  int x;
  int dir;
  sprite_t *sprite;

  x = duck[n].x;
  dir = duck[n].dir;
  switch (duck[n].mode) {
  case DUCK_BORED:
      if (dir & DIR_HORIZ) {
	  sprite = (dir == DIR_R) ? &SPRITE_DUCK_R : &SPRITE_DUCK_L;
      } else {
	  sprite = &SPRITE_DUCK_UP;
      }
      break;
  case DUCK_STEP:
      if (dir & DIR_HORIZ) {
	  sprite = (dir == DIR_R) ? &SPRITE_DUCK_R2 : &SPRITE_DUCK_L2;
      } else {
	  sprite = &SPRITE_DUCK_UP2;
      }
      break;
  case DUCK_EAT2:
  case DUCK_EAT4:
      if (dir == DIR_R) {
	 sprite = &SPRITE_DUCK_EAT_R;
      } else {
	 sprite = &SPRITE_DUCK_EAT_L;
	 x -= 8;
      }
      break;
  case DUCK_EAT3:
      if (dir == DIR_R) {
	 sprite = &SPRITE_DUCK_EAT_R2;
      } else {
	 sprite = &SPRITE_DUCK_EAT_L2;
	 x -= 8;
      }
      break;
  default:
      abort();
  };

  Do_RenderSprite(x, duck[n].y, sprite, PLANE_BLUE);
}

static void ErasePlayer(void)
{
  Do_RenderSprite(player_x, player_y, player_sprite, PLANE_YELLOW);
}

static void DrawPlayer(void)
{
  sprite_t *const *ps;
  int frame;

  if (player_face == 0) {
      ps = sprite_player_up;
      frame = player_partial_y >> 1;
  } else {
      if ((player_face & 0x80) != 0)
	ps = sprite_player_l;
      else
	ps = sprite_player_r;
      frame = player_partial_x >> 1;
  }
  if (player_mode != 1) {
      if (move_x == 0)
	frame = 0;
  } else {
      if (move_y == 0)
	frame = 0;
  }
  player_sprite = ps[frame];
  Do_RenderSprite(player_x, player_y, player_sprite, PLANE_YELLOW);
}

static void DrawLastLife(void)
{
  int tmp;
  tmp = lives[current_player];
  if (tmp >= 9)
      return;
  tmp = (tmp << 2) + (current_player * 0x22) + 0xd + 0xa;
  Do_RenderSprite(tmp, 0xee, &SPRITE_HAT, 4);
}

static void StartLevel(void)
{
  int i;
  if (have_lift) {
      lift_y1 = 8;
      lift_y2 = 0x5a;
      current_lift = 0;
      Do_RenderSprite(lift_x, lift_y1, &SPRITE_LIFT, COLOR_LIFT);
      Do_RenderSprite(lift_x, lift_y2, &SPRITE_LIFT, COLOR_LIFT);
  }
  big_duck_x = 4;
  big_duck_y = 0xcc;
  big_duck_dx = big_duck_dy = 0;
  big_duck_frame = 0;
  big_duck_dir = 0;
  DrawBigDuck();
  if ((current_level >> 3) == 1) {
      num_ducks = 0;
  }
  if (current_level >= 24) {
      num_ducks = 5;
  }
  for (i = 0; i < num_ducks; i++) {
      duck[i].x = duck[i].tile_x << 3;
      duck[i].y = (duck[i].tile_y << 3) + 0x14;
      duck[i].mode = DUCK_BORED;
      duck[i].dir = 2;
      DrawDuck(i);
  }
  /* Delay(3) */
  player_x = 0x3c;
  player_y = 0x20;
  DrawPlayer();
  player_tilex = 7;
  player_tiley = 2;
  player_partial_x = 7;
  player_partial_y = 0;
  player_mode = 0;
  player_face = 1;
  DrawLastLife();
}

/* Returns nonzero if blocked.  */
static int MoveSideways(void)
{
  int tmp, x, y;
  tmp = (int8_t)move_x;
  if (tmp == 0)
    return 0;
  if (tmp < 0) {
      if (player_x == 0)
	return 1;
      if (player_partial_x >= 2)
	return 0;
      if (move_y == 2)
	return 0;

      x = player_tilex - 1;
      y = player_tiley;
      tmp = player_partial_y + (int8_t)move_y;
      if (tmp < 0)
	y--;
      else if (tmp >= 8)
	y++;
      if (Do_ReadMap(x, y) == 1)
	return 1;
      if ((move_y & 0x80) == 0)
	return 0;
      x = player_tilex - 1;
      y++;
      return (Do_ReadMap(x, y) == 1);
  }
  tmp = player_x;
  if (tmp >= 0x98)
    return 1;
  if (player_partial_x < 5)
    return 0;
  if (move_y == 2)
    return 0;
  x = player_tilex + 1;
  y = player_tiley;
  tmp = player_partial_y + (int8_t)move_y;
  if (tmp < 0)
    y--;
  else if (tmp >= 8)
    y++;
  if (Do_ReadMap(x, y) == 1)
    return 1;
  if ((move_y & 0x80) == 0)
    return 0;
  x = player_tilex + 1;
  y++;
  return (Do_ReadMap(x, y) == 1);
}

static void RemoveGrain(int x, int y)
{
  Do_InitTile(x, y, &SPRITE_GRAIN, 0, COLOR_GRAIN);
}

static void ScoreChange(int n, int oldval, int newval)
{
    int x;
    player_data->score[n] = newval;
    if (n < 2)
	return;
    x = (current_player * 0x22) + 0x12 + n * 5;;
    Do_RenderDigit(x, 0xf7, oldval);
    Do_RenderDigit(x, 0xf7, newval);
}

static void AddScore(int n, int val)
{
    int oldval;

    while (n >= 0) {
	oldval = player_data->score[n];
	val += oldval;
	if (n == 3)
	  extra_life++;
	if (val < 10) {
	    ScoreChange(n, oldval, val);
	    return;
	}
	ScoreChange(n, oldval, val - 10);
	val = 1;
	n--;
    }
}
/*        N    T  PI1  PI2  PI3  PN1  PN2  PN3   AA   AD   AS   AR  ALA  ALD */
/* E1: 0x01 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x7e 0xce 0x00 0x00 0x64 0x00
 * E2: 0x02 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x7e 0xfe 0x00 0xfb 0x7e 0x64
 * E3: 0x03 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x32 0x00 0x00 0xe7 0x64 0x00
 */
static void beep(int tmp) /* 0x0c98 */
{
  /* channel = 13 (Flush), note = 1, pitch = tmp, duration = 0x0001 */
  sound_start(tmp, 1);
}

static void squidge(int tmp) /* 0x0ca8 */
{
  /* 0001 0003 0000 0004 */
  sound_start(tmp, 0);
}

static void AnimatePlayer(void)
{
    int tmp;
    int x, y;

    ErasePlayer();
    player_x += move_x;
    tmp = (int8_t)(player_partial_x + move_x);
    if (tmp < 0)
      player_tilex--;
    if (tmp >= 8)
      player_tilex++;
    player_partial_x = tmp & 7;
    player_y += move_y;
    tmp = (int8_t)(player_partial_y + move_y);
    if (tmp < 0)
      player_tiley--;
    if (tmp >= 8)
      player_tiley++;
    player_partial_y = tmp & 7;
    DrawPlayer();
    x = player_tilex;
    y = player_tiley;
    if (player_partial_y >= 4)
      y++;
    tmp = Do_ReadMap(x, y);
    if ((tmp & 0x0c) == 0)
      return;
    if ((tmp & 0x08) == 0) {
	/* Got egg */
	eggs_left--;
	/* SQUIDGE(6) */
	squidge(6);
	tmp >>= 4;
	player_data->egg[tmp]--;
	Do_InitTile(x, y, &SPRITE_EGG, 0, COLOR_EGG);
	tmp = (current_level >> 2) + 1;
	if (tmp >= 10)
	  tmp = 10;
	AddScore(5, tmp);
    } else {
	/* Got grain */
	/* SQUIDGE(5) */
	squidge(5);
	tmp >>= 4;
	player_data->grain[tmp]--;
	RemoveGrain(x, y);
	AddScore(6, 5);
	bonus_hold = 14;
    }
}

static void MovePlayer(void)
{
  int x, y, tmp;
  int tmp2;
  int y1, y2;

  move_x = 0;
  move_y = 0;
  if (buttons & 0x01) {
      move_x++;
  }
  if (buttons & 0x02) {
      move_x--;
  }
  if (buttons & 0x04) {
      move_y--;
  }
  if (buttons & 0x08) {
      move_y++;
  }
  move_y <<= 1;
  switch (player_mode) {
  case 2: /* Jump */
do_jump:
      move_x = player_slide;
      tmp2 = move_y;
      tmp = player_fall >> 2;
      if (tmp >= 6)
	tmp = 6;
      tmp ^= 0xff;
      tmp += 3;
      move_y = tmp;
      player_fall++;
      if (player_y == 0xdc) {
	  move_y = 0xff;
	  player_fall = 0x0c;
	  goto label_2062;
      }
      tmp = (int8_t)(player_partial_x + move_x);
      if (tmp != 3)
	  goto label_2016;
      if (tmp2 == 0)
	goto label_2016;
      /* Grab lader */
      if ((tmp2 & 0x80) == 0) {
	  x = player_tilex;
	  y = player_tiley + 1;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 2) == 0) {
	      if (player_partial_y >= 4)
		y++;
	      tmp = Do_ReadMap(x, y);
	      if ((tmp & 2) == 0)
		goto label_2016;
	  }
	  player_mode = 1;
	  tmp = player_partial_y + move_y;
	  if (tmp & 1)
	      move_y++;
	  goto label_20cd;
      }
      x = player_tilex;
      y = player_tiley;
      tmp = Do_ReadMap(x, y);
      if ((tmp & 2) == 0)
	goto label_2016;
      x = player_tilex;
      y = player_tiley + 1;
      tmp = Do_ReadMap(x, y);
      if ((tmp & 2) == 0)
	goto label_2016;
      player_mode = 1;
      tmp = player_partial_y + move_y;
      if (tmp & 1)
	  move_y--;
      goto label_20cd;
label_2016:
      tmp = (uint8_t)(move_y + player_partial_y);
      if (tmp == 0) {
	  x = player_tilex;
	  y = player_tiley - 1;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 1) != 0)
	    player_mode = 0;
      } else if ((tmp & 0x80) == 0) {
	  if (tmp != 8)
	    goto label_2062;
	  x = player_tilex;
	  y = player_tiley;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 1) != 0)
	    player_mode = 0;
      } else {
	  x = player_tilex;
	  y = player_tiley - 1;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 1) != 0) {
	      player_mode = 0;
	      move_y = -player_partial_y;
	  }
      }
label_2062:
      if (!have_lift)
	goto label_20bf;
      tmp = (uint8_t)(lift_x - 1);
      if (tmp >= player_x)
	goto label_20bf;
      tmp = (uint8_t)(tmp + 0x0a);
      if (tmp < player_x)
	goto label_20bf;
      y1 = player_y - 0x11;
      y2 = player_y - 0x13 + (int8_t)move_y;
      tmp = lift_y1;
      if (tmp > y1 || tmp < y2) {
	  tmp = lift_y2;
	  if (tmp != y1)
	    {
	      if (tmp >= y1)
		goto label_20bf;
	      if (tmp < y2)
		goto label_20bf;
	    }
	  if (current_lift == 0)
	    tmp++;
      } else {
	  if (current_lift != 0)
	    tmp++;
      }
      tmp -= y1;
      move_y = tmp + 1;
      player_fall = 0;
      player_mode = 4;
      goto label_20cd;
label_20bf:
      if (MoveSideways()) {
	  move_x = -move_x;
	  player_slide = move_x;
      }
label_20cd:
      break;
  case 3: /* Fall */
      player_fall++;
      tmp = player_fall;
      if (tmp < 4) {
	  move_x = player_slide;
	  move_y = 0xff;
      } else {
	  move_x = 0;
	  tmp = player_fall >> 2;
	  if (tmp >= 4)
	    tmp = 3;
	  move_y = ~tmp;
      }
      tmp = (int8_t)(move_y + player_partial_y);
      if (tmp == 0) {
	  x = player_tilex;
	  y = player_tiley - 1;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 1) != 0)
	    player_mode = 0;
      } else if (tmp < 0) {
	  x = player_tilex;
	  y = player_tiley - 1;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 1) != 0) {
	      player_mode = 0;
	      move_y = -player_partial_y;
	  }
      }
      break;
  case 1: /* Climb */
      if ((buttons & 0x10) != 0)
	goto start_jump;
      if (move_x != 0 && player_partial_y == 0) {
	  x = player_tilex;
	  y = player_tiley - 1;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 1) == 0)
	      goto do_climb;
	  move_y = 0;
	  player_mode = 0;
      } else {
do_climb:
	  move_x = 0;
	  if (move_y != 0 && player_partial_y == 0) {
	      if ((move_y & 0x80) == 0) {
		  x = player_tilex;
		  y = player_tiley + 2;
		  tmp = Do_ReadMap(x, y);
		  if ((tmp & 2) == 0)
		    move_y = 0;
	      } else {
		  x = player_tilex;
		  y = player_tiley - 1;
		  tmp = Do_ReadMap(x, y);
		  if ((tmp & 2) == 0)
		    move_y = 0;
	      }
	  }
      }
      player_face = 0;
      break;
  case 4: /* On lift */
      if ((buttons & 0x10) != 0)
	goto start_jump;
      if (lift_x - 1 >= player_x || lift_x + 9 < player_x) {
	  player_fall = 0;
	  player_slide = 0;
	  player_mode = 3;
      }
      move_y = 1;
      if (move_x != 0)
	player_face = move_x;
      if (MoveSideways()) {
	  move_x = 0;
      }
      if (player_y >= 0xdc)
	is_dead++;
      break;
  case 0:
      /* Walk */
      if (buttons & 0x10) {
start_jump:
	  player_fall = 0;
	  player_mode = 2;
	  tmp = move_x;
	  player_slide = tmp;
	  if (tmp != 0)
	    player_face = tmp;
	  goto do_jump;
      }
      if (move_y) {
	  if (player_partial_x == 3) {
	      x = player_tilex;
	      if ((move_y & 0x80) == 0)
		  y = player_tiley + 2;
	      else 
		  y = player_tiley - 1;
	      tmp = Do_ReadMap(x, y);
	      if ((tmp & 2) != 0) {
		  move_x = 0;
		  player_mode = 1;
		  break;
	      }
	  }
	  move_y = 0;
      }
      tmp = (int8_t)(player_partial_x + move_x);
      x = player_tilex;
      if (tmp < 0)
	x--;
      else if (tmp >= 8)
	x++;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y);
      tmp &= 1;
      if (tmp == 0) {
	  /* Walk off edge */
	  int n;
	  n = (move_x + player_partial_x) & 7;
	  if (n < 4) {
	      x = 1;
	      y = 1;
	  } else {
	      y = 0;
	      x = 0xff;
	  }
	  player_slide = x;
	  player_fall = y;
	  player_mode = 3;
      }
      if (MoveSideways()) {
	  move_x = 0;
      }
      if (move_x) {
	  player_face = move_x;
      }
      break;
  default:
      abort();
  }
  AnimatePlayer();
}

static void MakeSound(void)
{
  int tmp;
  if (!(move_x || move_y))
    return;
  if ((duck_timer & 1) != 0)
    return;
  tmp = player_mode;
  if (tmp == 0) {
      tmp = 64;
      goto label_0c8b;
  }
  if (tmp == 1) {
      tmp = 96;
      goto label_0c8b;
  }
  if (tmp != 2) /*0x0c5a */
    goto label_0c76;
  tmp = player_fall;
  if (tmp >= 0x0b) {
      tmp = (uint8_t)(0xbe - (player_fall * 2));
      goto label_0c8b;
  }
  tmp = (uint8_t)(0x96 + (player_fall * 2));
  goto label_0c8b;
label_0c76:
  if (tmp == 3) {
    tmp = (uint8_t)(0x6e - (player_fall * 2));
    goto label_0c8b;
  }
  if (move_x == 0)
    return;
  tmp = 0x64;
label_0c8b:
  /* BEEP(tmp) */
  beep(tmp);
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
  Do_RenderSprite(lift_x, y, &SPRITE_LIFT, COLOR_LIFT);
  y += 2;
  if (y == 0xe0)
    y = 6;
  Do_RenderSprite(lift_x, y, &SPRITE_LIFT, COLOR_LIFT);
  if (current_lift == 0) {
      lift_y1 = y;
  } else {
      lift_y2 = y;
  }
  current_lift = !current_lift;
}

static int popcount(int val)
{
  return __builtin_popcount(val);
}

static void FrobRandom(void)
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
  do {
      DrawBonus(n);
      bonus[n]--;
      flag = ((bonus[n] & 0x80) != 0);
      if (flag) {
	  bonus[n] = 9;
      }
      DrawBonus(n);
      n--;
  } while (flag);
  if (bonus[0] + bonus[1] + bonus[2] == 0)
    zero_bonus = 1;
}

static void MoveDucks(void)
{
  int tmp;
  int y;
  int x;
  int flag;
  int tmp2;
  int newdir;
  duckinfo_t *this_duck;

  duck_timer++;
  if (duck_timer == 8) {
      /* Big Duck.  */
      duck_timer = 0;
      DrawBigDuck();
      if (have_big_duck) {
	  tmp = (uint8_t)(big_duck_x + 4);
	  if (tmp < player_x) {
	      if (big_duck_dx < 5)
		big_duck_dx++;
	      big_duck_dir = 0;
	  } else {
	      if (big_duck_dx > -5)
		big_duck_dx--;
	      big_duck_dir = 1;
	  }
	  tmp = player_y + 4;
	  if (tmp >= big_duck_y) {
	      if (big_duck_dy < 5)
		big_duck_dy++;
	  } else {
	      if (big_duck_dy > -5)
		big_duck_dy--;
	  }
	  tmp = (uint8_t)(big_duck_y + big_duck_dy);
	  if (tmp < 0x28)
	    big_duck_dy = -big_duck_dy;
	  tmp = (uint8_t)(big_duck_x + big_duck_dx);
	  if (tmp >= 0x90)
	    big_duck_dx = -big_duck_dx;
      }
      big_duck_x += big_duck_dx;
      big_duck_y += big_duck_dy;
      big_duck_frame ^= 1;
      DrawBigDuck();
      return;
  }
  if (duck_timer == 4) {
      /* Update bonus/timer.  */
      if (bonus_hold) {
	  bonus_hold--;
	  return;
      }
      x = 2;
      do {
	  DrawTimer(x);
	  timer_ticks[x]--;
	  flag = (timer_ticks[x] & 0x80) != 0;
	  if (flag)
	    timer_ticks[x] = 9;
	  DrawTimer(x);
	  x--;
      } while (flag);
      tmp = (uint8_t)(timer_ticks[0] + timer_ticks[1] + timer_ticks[2]);
      if (tmp == 0) {
	  is_dead++;
	  return;
      }
      tmp = timer_ticks[2];
      if (tmp != 0 && tmp != 5)
	return;
      if (zero_bonus)
	return;
      ReduceBonus();
      return;
  }
  if (current_duck == 0)
    current_duck = duck_speed;
  else
    current_duck--;
  if (current_duck >= num_ducks)
    return;
  DrawDuck(current_duck);
  /* Move little duck.  */
  this_duck = &duck[current_duck];
  if (this_duck->mode >= DUCK_EAT1) {
      /* Eat grain.  */
      if (this_duck->mode == DUCK_EAT2) {
	  x = duck[current_duck].tile_x - 1;
	  y = duck[current_duck].tile_y;
	  if ((duck[current_duck].dir & DIR_L) == 0)
	    x += 2;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 8) != 0) {
	      player_data->grain[tmp >> 4]--;
	      RemoveGrain(x, y);
	  }
      }
  } else if (this_duck->mode == DUCK_BORED) {
      /* Figure out which way to go next.  */
      x = duck[current_duck].tile_x;
      y = duck[current_duck].tile_y;
      newdir = 0;
      tmp = Do_ReadMap(x - 1, y - 1);
      if ((tmp & 1) != 0)
	newdir = DIR_L;
      tmp = Do_ReadMap(x + 1, y - 1);
      if ((tmp & 1) != 0)
	newdir |= DIR_R;
      tmp = Do_ReadMap(x, y - 1);
      if ((tmp & 2) != 0)
	newdir |= DIR_DOWN;
      tmp = Do_ReadMap(x, y + 2);
      if ((tmp & 2) != 0)
	newdir |= DIR_UP;
      if (popcount(newdir) != 1) {
	  tmp = this_duck->dir;
	  if (tmp & DIR_HORIZ) {
	      tmp ^= 0xfc;
	  } else {
	      tmp ^= 0xf3;
	  }
	  newdir &= tmp;
      }
      if (popcount(newdir) != 1) {
	  tmp2 = newdir;
	  do {
	      FrobRandom();
	      newdir = rand_low & tmp2;
	  } while (popcount(newdir) != 1);
      }
      duck[current_duck].dir = newdir;
      /* Check for grain to eat.  */
      tmp = this_duck->dir;
      if (tmp & DIR_HORIZ) {
	  if (tmp == DIR_L)
	    tmp = Do_ReadMap(x - 1, y);
	  else
	    tmp = Do_ReadMap(x + 1, y);
	  tmp &= 8;
	  if (tmp != 0) {
	      this_duck->mode = DUCK_EAT1;
	  }
      }
  }
  if (this_duck->mode >= DUCK_EAT1) {
      /* Eating.  */
      if (this_duck->mode == DUCK_EAT4)
	this_duck->mode = DUCK_BORED;
      else
	this_duck->mode++;
      DrawDuck(current_duck);
      return;
  }
  /* Walking.  */
  if (this_duck->mode == DUCK_STEP) {
      this_duck->mode = DUCK_BORED;
      flag = 1;
  } else {
      this_duck->mode = DUCK_STEP;
      flag = 0;
  }
  switch (this_duck->dir) {
  case DIR_L:
      duck[current_duck].x -= 4;
      duck[current_duck].tile_x -= flag;
      break;
  case DIR_R:
      duck[current_duck].x += 4;
      duck[current_duck].tile_x += flag;
      break;
  case DIR_UP:
      duck[current_duck].y += 4;
      duck[current_duck].tile_y += flag;
      break;
  case DIR_DOWN:
      duck[current_duck].y -= 4;
      duck[current_duck].tile_y -= flag;;
      break;
  default:
      abort();
  }
  DrawDuck(current_duck);
  return;
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
    int n;

    /* Little ducks */
    for (n = 0; n < num_ducks; n++) {
	if ((uint8_t)((duck[n].x - player_x) + 5) < 0x0b
	    && (uint8_t)((duck[n].y - 1) - player_y + 0xe) < 0x1d)
	  is_dead++;
    }
    /* Big duck */
    if (!have_big_duck)
      return;
    if ((uint8_t)(big_duck_x + 4 - player_x + 5) >= 0x0b)
      return;
    if ((uint8_t)(big_duck_y - 5 - player_y + 0x0e) >= 0x1d)
      return;
    is_dead++;
}

static void SavePlayerState(void)
{
    int i;
    level[current_player] = current_level;
    for (i = 0; i < 4; i++)
      player_data->bonus[i] = bonus[i];
}

static void ResetPlayer(void)
{
  int a;
  int i;
  a = current_level + 1;
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

static void PlayTune(int addr)
{
  /* Play tune 0x2f7c */
}

static void start_game(void)
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
      ResetPlayer();
  }
  RestorePlayerState();
}

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

static void ClearScreen(void)
{
  memset(pixels, 0, 160*256);
}

void run_game(void)
{
  int tmp;

new_game:
  start_game();
next_player:
  /* "Get Ready" */
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

  if ((buttons & 0x80) != 0)
    goto new_game;
  if (is_dead != 0 || player_y < 0x11) {
      /* Died */
      SavePlayerState();
      PlayTune(0x2fa6);
      if (--lives[current_player] == 0) {
	  /* Clear Screen */
	  /* "Game Over" */
	  /* Highscores.  */
	  if (--active_players == 0)
	    goto new_game;
      }
      /* Select next player. */
      tmp = current_player;
      do {
	  tmp = (tmp + 1) & 3;
      } while (tmp >= num_players || lives[tmp] == 0);
      current_player = tmp;
      RestorePlayerState();
      goto next_player;
  }
  if (eggs_left == 0 || cheat) {
      /* Level complete */
      while (!zero_bonus) {
	  AddScore(6, 1);
	  ReduceBonus();
	  MaybeAddExtraLife();
	  tmp = timer_ticks[3];
	  if (tmp == 0 || tmp == 5) {
	      /* sound(0xcb0) - 0010 0001 0004 0001 */
	  }
	  /* Render Screen? */
      }
      /* Advance to next level */
      cheat = 0;
      zero_bonus = 0;
      current_level++;
      SavePlayerState();
      ResetPlayer();
      RestorePlayerState();
      goto next_level;
  }
  goto next_frame;
}
