#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chuckie.h"
#include "data.h"

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

uint8_t pixels[160 * 256];

static void Do_RenderSprite(int x, int y, int sprite, int color)
{
    const uint8_t *src;
    uint8_t srcbits;
    int bits;
    int sx;
    int sy;
    int i;
    uint8_t *dest;

    y = y ^ 0xff;
    sx = sprites[sprite].x;
    sy = sprites[sprite].y;
    src = sprites[sprite].data;
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
    Do_RenderSprite(x, 0xee, SPRITE_HAT, 4);
    x += 4;
  }
}


static void DrawHUD(void)
{
  int tmp;

  Do_RenderSprite(0, 0xf8, SPRITE_SCORE, 2);
  tmp = current_player * 0x22 + 0x1b;
  Do_RenderSprite(tmp, 0xf8, SPRITE_BLANK, 2);
  for (tmp = 0; tmp < num_players; tmp++) {
      DrawLives(tmp);
  }
  Do_RenderSprite(0, 0xe8, SPRITE_PLAYER, 2);
  Do_RenderSprite(0x1b, 0xe7, current_player + SPRITE_1, 2);
  Do_RenderSprite(0x24, 0xe8, SPRITE_LEVEL, 2);

  tmp = current_level + 1;
  Do_RenderDigit(0x45, 0xe7, tmp % 10);
  tmp /= 10;
  Do_RenderDigit(0x40, 0xe7, tmp % 10);
  if (tmp > 10)
    Do_RenderDigit(0x3b, 0xe7, tmp / 10);

  Do_RenderSprite(0x4e, 0xe8, SPRITE_BONUS, 2);

  Do_RenderDigit(0x66, 0xe7, bonus[0]);
  Do_RenderDigit(0x6b, 0xe7, bonus[1]);
  Do_RenderDigit(0x70, 0xe7, bonus[2]);
  Do_RenderDigit(0x75, 0xe7, 0);
  Do_RenderSprite(0x7e, 0xe8, SPRITE_TIME, 2);

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
	    Do_InitTile(x, y, 1, 1, 3);
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
		Do_RenderSprite(x << 3, (y << 3) | 7, SPRITE_WALL, 3);
	    Do_InitTile(x, y, SPRITE_LADDER, tmp | 2, 2);
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
	    Do_InitTile(x, y, SPRITE_EGG, (i << 4) | 4, 1);
	    eggs_left++;
	}
    }

    for (i = 0; i < num_grain; i++) {
	x = *(p++);
	y = *(p++);
	if (player_data->grain[i] == 0) {
	    Do_InitTile(x, y, 4, (i << 4) | 8, 2);
	}
    }

    Do_RenderSprite(0, 0xdc, have_big_duck ? SPRITE_CAGE_OPEN
					   : SPRITE_CAGE_CLOSED, 4);

    for (i = 0; i < 5 ; i++) {
	duck[i].tile_x = *(p++);
	duck[i].tile_y = *(p++);
    }
}

/* DrawBigDuck */
static void DrawBigDuck(void)
{
  Do_RenderSprite(big_duck_x, big_duck_y,
		  big_duck_sprite + SPRITE_BIGDUCK_R1, 4);
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
  if (tmp >= 9)
      return;
  tmp = (tmp << 2) + (current_player * 0x22) + 0xd + 0xa;
  Do_RenderSprite(tmp, 0xee, SPRITE_HAT, 4);
}

static void StartLevel(void)
{
  int i;
  if (have_lift) {
      lift_y1 = 8;
      lift_y2 = 0x5a;
      current_lift = 0;
      Do_RenderSprite(lift_x, lift_y1, 5, 1);
      Do_RenderSprite(lift_x, lift_y2, 5, 1);
  }
  big_duck_x = 4;
  big_duck_y = 0xcc;
  big_duck_dx = big_duck_dy = 0;
  big_duck_sprite = 0;
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
      duck[i].mode = 0;
      duck[i].sprite = 0;
      duck[i].dir = 2;
      DrawDuck(i);
  }
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
  Do_InitTile(x, y, 4, 0, 2);
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

static void AnimatePlayer(void)
{
    int tmp;
    int x, y;

    DrawPlayer();
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
    if (player_face == 0) {
	player_sprite = SPRITE_PLAYER_UP;
	tmp = player_partial_y >> 1;
    } else {
	if ((player_face & 0x80) != 0)
	  player_sprite = SPRITE_PLAYER_L;
	else
	  player_sprite = SPRITE_PLAYER_R;
	tmp = player_partial_x >> 1;
    }
    if (tmp > 1)
      tmp = (tmp & 1) << 1;

    if (player_mode != 1) {
	if (move_x == 0)
	  tmp = 0;
    } else {
	if (move_y == 0)
	  tmp = 0;
    }
    player_sprite += tmp;
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
	/* BEEP(6) */
	tmp >>= 4;
	player_data->egg[tmp]--;
	Do_InitTile(x, y, 3, 0, 1);
	tmp = (current_level >> 2) + 1;
	if (tmp >= 10)
	  tmp = 10;
	AddScore(5, tmp);
    } else {
	/* Got grain */
	/* BEEP(5) */
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
      if (tmp == y1)
	goto label_208f;
      if (tmp >= y1)
	goto label_2099;
      if (tmp < y2)
	goto label_2099;
label_208f:
      if (current_lift != 0)
	tmp++;
      goto label_20ac;
label_2099:
      tmp = lift_y2;
      if (tmp == y1)
	goto label_20a5;
      if (tmp >= y1)
	goto label_20bf;
      if (tmp < y2)
	goto label_20bf;
label_20a5:
      if (current_lift == 0)
	tmp++;
label_20ac:
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
      if (tmp == 0)
	goto label_212b;
      if (tmp >= 0)
	goto label_213b;
      x = player_tilex;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y);
      if ((tmp & 1) == 0)
	goto label_213b;
      player_mode = 0;
      move_y = -player_partial_y;
      goto label_213b;
label_212b:
      x = player_tilex;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y);
      if ((tmp & 1) != 0)
	player_mode = 0;
label_213b:
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
  tmp = (uint8_t)(0x96 - (player_fall * 2));
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
  Do_RenderSprite(lift_x, y, 5, 1);
  y += 2;
  if (y == 0xe0)
    y = 6;
  Do_RenderSprite(lift_x, y, 5, 1);
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
label_26e0:
  DrawBonus(n);
  bonus[n]--;
  flag = ((bonus[n] & 0x80) != 0);
  if (flag) {
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
  if (duck_timer == 4)
    goto label_269d;
  goto label_24b5;
label_2420:
  duck_look = big_duck_sprite & 2;
  if (!have_big_duck)
    goto label_2494;
  tmp = (uint8_t)(big_duck_x + 4);
  if (tmp >= player_x)
    goto label_2444;
  if (big_duck_dx < 5)
    big_duck_dx++;
  duck_look = 0;
  goto label_2452;
label_2444:
  if (big_duck_dx > -5)
    big_duck_dx--;
  duck_look = 2;
label_2452:
  tmp = player_y + 4;
  if (tmp < big_duck_y)
    goto label_2468;
  if (big_duck_dy < 5)
    big_duck_dy++;
  goto label_2472;
label_2468:
  if (big_duck_dy > -5)
    big_duck_dy--;
label_2472:
  tmp = (uint8_t)(big_duck_y + big_duck_dy);
  if (tmp >= 0x28)
    goto label_2483;
  big_duck_dy = -big_duck_dy;
label_2483:
  tmp = (uint8_t)(big_duck_x + big_duck_dx);
  if (tmp < 0x90)
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
  if (current_duck >= num_ducks)
    return;
  /* Move little duck.  */
  tmp = duck[current_duck].mode;
  if (tmp == 1)
    goto label_25ef;
  if (tmp >= 1)
    goto label_25b6;
  x = duck[current_duck].tile_x;
  y = duck[current_duck].tile_y;
  newdir = 0;
  tmp = Do_ReadMap(x - 1, y - 1);
  if ((tmp & 1) != 0)
    newdir = 1;
  tmp = Do_ReadMap(x + 1, y - 1);
  if ((tmp & 1) != 0)
    newdir |= 2;
  tmp = Do_ReadMap(x, y - 1);
  if ((tmp & 2) != 0)
    newdir |= 8;
  tmp = Do_ReadMap(x, y + 2);
  if ((tmp & 2) != 0)
    newdir |= 4;
  if (popcount(newdir) == 1) {
      duck[current_duck].dir = newdir;
      goto label_257b;
  }
  tmp = duck[current_duck].dir;
  if (tmp < 4) {
      tmp ^= 0xfc;
  } else {
      tmp ^= 0xf3;
  }
  newdir &= tmp;
  if (popcount(newdir) == 1) {
      duck[current_duck].dir = newdir;
      goto label_257b;
  }
  tmp2 = newdir;
label_2564:
  FrobRandom();
  newdir = rand_low & tmp2;
  if (popcount(newdir) != 1)
    goto label_2564;
  duck[current_duck].dir = newdir;
label_257b:
  tmp = duck[current_duck].dir;
  tmp &= 3;
  if (tmp == 0)
    goto label_25ef;
  tmp &= 1;
  if (tmp == 0)
    goto label_2593;
  tmp = Do_ReadMap(x - 1, y);
  goto label_259b;
label_2593:
  tmp = Do_ReadMap(x + 1, y);
label_259b:
  tmp &= 8;
  if (tmp == 0)
    goto label_25ef;
  tmp = 2;
  duck[current_duck].mode = tmp;
  goto label_25ef;
label_25b6:
  if (tmp != 4)
    goto label_25ef;
  x = duck[current_duck].tile_x - 1;
  y = duck[current_duck].tile_y;
  if ((duck[current_duck].dir & 1) == 0)
    x += 2;
  tmp = Do_ReadMap(x, y);
  if ((tmp & 8) == 0)
    goto label_25ef;
  player_data->grain[tmp >> 4]--;
  RemoveGrain(x, y);
label_25ef:
  DrawDuck(current_duck);
  tmp = duck[current_duck].mode;
  if (tmp >= 2)
    goto label_2675;
  tmp = duck[current_duck].dir;
  if ((tmp & 1) != 0)
    goto label_2633;
  if ((tmp & 2) != 0)
    goto label_2649;
  if ((tmp & 4) != 0)
    goto label_261d;
  duck[current_duck].y -= 4;
  tmp = duck[current_duck].mode;
  if (tmp != 0)
    duck[current_duck].tile_y--;
  tmp = 4;
  goto label_265f;
label_261d:
  duck[current_duck].y += 4;
  tmp = duck[current_duck].mode;
  if (tmp != 0)
    duck[current_duck].tile_y++;
  tmp = 4;
  goto label_265f;
label_2633:
  duck[current_duck].x -= 4;
  tmp = duck[current_duck].mode;
  if (tmp != 0)
    duck[current_duck].tile_x--;
  tmp = 2;
  goto label_265f;
label_2649:
  duck[current_duck].x += 4;
  tmp = duck[current_duck].mode;
  if (tmp != 0)
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
  if (y == 1)
    tmp += 2;
  y = duck[current_duck].mode;
  if (y == 8)
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
  if (flag)
    timer_ticks[x] = 9;
  DrawTimer(x);
  x--;
  if (flag)
    goto label_26ac;
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
    int x;
    int tmp;
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
  if (eggs_left == 0) {
      /* Level complete */
      while (!zero_bonus) {
	  AddScore(6, 1);
	  ReduceBonus();
	  MaybeAddExtraLife();
	  tmp = timer_ticks[3];
	  if (tmp == 0 || tmp == 5) {
	      /* BEEP(?) */
	  }
	  /* Render Screen? */
      }
      /* Advance to next level */
      zero_bonus = 0;
      current_level++;
      SavePlayerState();
      ResetPlayer();
      RestorePlayerState();
      goto next_level;
  }
  goto next_frame;
}
