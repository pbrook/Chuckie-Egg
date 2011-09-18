/* Chuckie Egg.  Based on the original by A & F Software
   Written by Paul Brook
   Released under the GNU GPL v3.  */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chuckie.h"
#include "raster.h"

#define NUM_PLAYERS 1

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
int lift_x;
int lift_y[2];
static int current_lift;
int have_big_duck;
int duck_timer;
int big_duck_frame;
int big_duck_x;
static int big_duck_dx;
int big_duck_y;
static int big_duck_dy;
int big_duck_dir;
int num_ducks;
int current_duck;
int duck_speed;
uint8_t bonus[4];
uint8_t timer_ticks[3];
uint8_t level[4];
uint8_t levelmap[20 * 25];
player_mode_t player_mode = PLAYER_WALK;
static int player_fall;
static int player_slide;
int player_face;
int player_x;
int player_y;
static int player_tilex;
static int player_tiley;
static int player_partial_x;
static int player_partial_y;
int move_x;
int move_y;
uint8_t buttons;
uint8_t button_ack;
uint32_t rand_high;
uint8_t rand_low;

playerdata_t all_player_data[4];
#define player_data (&all_player_data[current_player])

duckinfo_t duck[5];

/* Movement code generates out of bounds reads.  */
static int Do_ReadMap(uint8_t x, uint8_t y)
{
  if (y >= 0x19 || x >= 0x14) {
      return 0;
  }
  return levelmap[x + y * 20];
}

static void Do_InitTile(int x, int y, int type)
{
    int old_type;

    old_type = levelmap[y * 20 + x];
    levelmap[y * 20 + x] = type;
    if (raster) {
	if (old_type)
	    raster->draw_tile(x, y, old_type);
	if (type)
	    raster->draw_tile(x, y, type);
    }
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

    i = current_level >> 4;
    if (i > 8)
	i = 8;
    timer_ticks[0] = 9 - i;
    timer_ticks[1] = 0;
    timer_ticks[2] = 0;

    if (raster)
	raster->draw_hud();

    p = levels[current_level & 7];
    num_walls = *(p++);
    num_ladders = *(p++);
    have_lift = *(p++);
    num_grain = *(p++);
    num_ducks = *(p++);
    for (i = 0; i < 20 * 25; i++)
      levelmap[i] = 0;

    while (num_walls--) {
	y = *(p++);
	x = *(p++);
	i = *(p++);
	while (x <= i) {
	    Do_InitTile(x, y, TILE_WALL);
	    x++;
	}
    }

    while (num_ladders--) {
	x = *(p++);
	y = *(p++);
	i = *(p++);
	while (y <= i) {
	    tmp = levelmap[x + y * 20];
	    Do_InitTile(x, y, TILE_LADDER | tmp);
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
	    Do_InitTile(x, y, (i << 4) | TILE_EGG);
	    eggs_left++;
	}
    }

    for (i = 0; i < num_grain; i++) {
	x = *(p++);
	y = *(p++);
	if (player_data->grain[i] == 0) {
	    Do_InitTile(x, y, (i << 4) | TILE_GRAIN);
	}
    }

    if (raster)
	raster->draw_cage (have_big_duck);

    for (i = 0; i < 5 ; i++) {
	duck[i].tile_x = *(p++);
	duck[i].tile_y = *(p++);
    }
}

static void DrawLastLife(void)
{
  if (raster)
    raster->draw_life(player_data->lives);
}

static void StartLevel(void)
{
  int i;
  if (have_lift) {
      lift_y[0] = 8;
      lift_y[1] = 0x5a;
      current_lift = 0;
      if (raster) {
	  raster->draw_lift(lift_x, lift_y[0]);
	  raster->draw_lift(lift_x, lift_y[1]);
      }
  }
  big_duck_x = 4;
  big_duck_y = 0xcc;
  big_duck_dx = big_duck_dy = 0;
  big_duck_frame = 0;
  big_duck_dir = 0;
  if (raster)
      raster->draw_big_duck();
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
      if (raster)
	  raster->draw_duck(i);
  }
  /* Delay(3) */
  player_x = 0x3c;
  player_y = 0x20;
  if (raster)
      raster->draw_player();
  player_tilex = 7;
  player_tiley = 2;
  player_partial_x = 7;
  player_partial_y = 0;
  player_mode = PLAYER_WALK;
  player_face = 1;
  button_ack = 0x1f;
  DrawLastLife();
}

/* Returns nonzero if blocked.  */
static int MoveSideways(void)
{
  int tmp, x, y;
  tmp = move_x;
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
      tmp = player_partial_y + move_y;
      if (tmp < 0)
	y--;
      else if (tmp >= 8)
	y++;
      if (Do_ReadMap(x, y) == 1)
	return 1;
      if (move_y >= 0)
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
  tmp = player_partial_y + move_y;
  if (tmp < 0)
    y--;
  else if (tmp >= 8)
    y++;
  if (Do_ReadMap(x, y) == 1)
    return 1;
  if (move_y >= 0)
    return 0;
  x = player_tilex + 1;
  y++;
  return (Do_ReadMap(x, y) == 1);
}

static void RemoveGrain(int x, int y)
{
  Do_InitTile(x, y, 0);
}

static void ScoreChange(int n, int oldval, int newval)
{
    player_data->score[n] = newval;
    if (raster)
	raster->draw_score(n, oldval, newval);
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

    if (raster)
	raster->erase_player();
    player_x += move_x;
    tmp = player_partial_x + move_x;
    if (tmp < 0)
      player_tilex--;
    if (tmp >= 8)
      player_tilex++;
    player_partial_x = tmp & 7;
    player_y += move_y;
    tmp = player_partial_y + move_y;
    if (tmp < 0)
      player_tiley--;
    if (tmp >= 8)
      player_tiley++;
    player_partial_y = tmp & 7;
    if (raster)
	raster->draw_player();
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
	Do_InitTile(x, y, 0);
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

static void PlayerGrabLadder(int want_move)
{
  int tmp;
  int x;
  int y;

  tmp = player_partial_x + move_x;
  if (tmp != 3)
      return;
  if (want_move == 0)
      return;
  if (want_move > 0) {
      x = player_tilex;
      y = player_tiley + 1;
      tmp = Do_ReadMap(x, y);
      if ((tmp & 2) == 0) {
	  if (player_partial_y >= 4)
	    y++;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 2) == 0)
	    return;
      }
      player_mode = PLAYER_CLIMB;
      tmp = player_partial_y + move_y;
      if (tmp & 1)
	  move_y++;
      return;
  }
  x = player_tilex;
  y = player_tiley;
  tmp = Do_ReadMap(x, y);
  if ((tmp & 2) == 0)
    return;
  x = player_tilex;
  y = player_tiley + 1;
  tmp = Do_ReadMap(x, y);
  if ((tmp & 2) == 0)
    return;
  player_mode = PLAYER_CLIMB;
  tmp = player_partial_y + move_y;
  if (tmp & 1)
      move_y--;
  return;
}

static void PlayerHitLift(void)
{
  int tmp;
  int y1;
  int y2;

  if (!have_lift)
    return;
  if ((lift_x > player_x) || (lift_x + 10 < player_x))
    return;
  y1 = player_y - 0x11;
  y2 = player_y - 0x13 + move_y;
  tmp = lift_y[0];
  if (tmp > y1 || tmp < y2) {
      tmp = lift_y[1];
      if (tmp != y1)
	{
	  if (tmp >= y1)
	    return;
	  if (tmp < y2)
	    return;
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
  player_mode = PLAYER_LIFT;
}

static void PlayerJump(void)
{
  int tmp;
  int tmp2;
  int x;
  int y;

  move_x = player_slide;
  tmp2 = move_y;
  tmp = player_fall >> 2;
  if (tmp >= 6)
    tmp = 6;
  move_y = 2 - tmp;
  player_fall++;
  if (player_y == 0xdc) {
      move_y = -1;
      player_fall = 0x0c;
  } else {
      PlayerGrabLadder(tmp2);
      if (player_mode == PLAYER_CLIMB)
	  return;
  }

  tmp = move_y + player_partial_y;
  if (tmp == 0) {
      x = player_tilex;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y);
      if ((tmp & 1) != 0)
	player_mode = PLAYER_WALK;
  } else if (tmp > 0) {
      if (tmp == 8) {
	  x = player_tilex;
	  y = player_tiley;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 1) != 0)
	    player_mode = PLAYER_WALK;
      }
  } else {
      x = player_tilex;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y);
      if ((tmp & 1) != 0) {
	  player_mode = PLAYER_WALK;
	  move_y = -player_partial_y;
      }
  }

  PlayerHitLift();
  if (player_mode == PLAYER_LIFT)
      return;

  if (MoveSideways()) {
      move_x = -move_x;
      player_slide = move_x;
  }
}

static void StartPlayerJump(void)
{
    int tmp;

    button_ack |= 0x10;
    player_fall = 0;
    player_mode = PLAYER_JUMP;
    tmp = move_x;
    player_slide = tmp;
    if (tmp != 0)
	player_face = tmp;
    PlayerJump();
}

static void MovePlayer(void)
{
  int x, y, tmp;

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
  case PLAYER_JUMP:
      PlayerJump();
      break;
  case PLAYER_FALL:
      player_fall++;
      tmp = player_fall;
      if (tmp < 4) {
	  move_x = player_slide;
	  move_y = -1;
      } else {
	  move_x = 0;
	  tmp = player_fall >> 2;
	  if (tmp > 3)
	    tmp = 3;
	  move_y = -(tmp + 1);
      }
      tmp = move_y + player_partial_y;
      if (tmp == 0) {
	  x = player_tilex;
	  y = player_tiley - 1;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 1) != 0)
	    player_mode = PLAYER_WALK;
      } else if (tmp < 0) {
	  x = player_tilex;
	  y = player_tiley - 1;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 1) != 0) {
	      player_mode = PLAYER_WALK;
	      move_y = -player_partial_y;
	  }
      }
      break;
  case PLAYER_CLIMB:
      if ((buttons & 0x10) != 0) {
	  StartPlayerJump();
	  break;
      }
      if (move_x != 0 && player_partial_y == 0) {
	  x = player_tilex;
	  y = player_tiley - 1;
	  tmp = Do_ReadMap(x, y);
	  if ((tmp & 1) != 0) {
	      move_y = 0;
	      player_mode = PLAYER_WALK;
	  }
      }
      if (player_mode != PLAYER_WALK) {
	  move_x = 0;
	  if (move_y != 0 && player_partial_y == 0) {
	      if (move_y >= 0) {
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
      if ((buttons & 0x10) != 0) {
	  StartPlayerJump();
	  break;
      }
      if (lift_x > player_x || lift_x + 9 < player_x) {
	  player_fall = 0;
	  player_slide = 0;
	  player_mode = PLAYER_FALL;
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
  case PLAYER_WALK:
      if (buttons & 0x10) {
	  StartPlayerJump();
	  break;
      }
      if (move_y) {
	  if (player_partial_x == 3) {
	      x = player_tilex;
	      if (move_y >= 0)
		  y = player_tiley + 2;
	      else 
		  y = player_tiley - 1;
	      tmp = Do_ReadMap(x, y);
	      if ((tmp & 2) != 0) {
		  move_x = 0;
		  player_mode = PLAYER_CLIMB;
		  break;
	      }
	  }
	  move_y = 0;
      }
      tmp = player_partial_x + move_x;
      x = player_tilex;
      if (tmp < 0)
	x--;
      else if (tmp >= 8)
	x++;
      y = player_tiley - 1;
      tmp = Do_ReadMap(x, y);
      if ((tmp & 1) == 0) {
	  /* Walk off edge */
	  int n;
	  n = (move_x + player_partial_x) & 7;
	  if (n < 4) {
	      x = 1;
	      y = 1;
	  } else {
	      y = 0;
	      x = -1;
	  }
	  player_slide = x;
	  player_fall = y;
	  player_mode = PLAYER_FALL;
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
  switch (player_mode) {
  case 0:
      tmp = 64;
      break;
  case 1:
      tmp = 96;
      break;
  case 2:
      tmp = player_fall;
      if (tmp >= 0x0b) {
	  tmp = 0xbe - (player_fall * 2);
      } else {
	  tmp = 0x96 + (player_fall * 2);
      }
      break;
  case 3:
      tmp = 0x6e - (player_fall * 2);
      break;
  case 4:
      if (move_x == 0)
	return;
      tmp = 0x64;
      break;
  default:
      abort();
  }
  beep(tmp);
}

/* MoveLift */
static void MoveLift(void)
{
  int y;

  if (!have_lift)
    return;
  y = lift_y[current_lift];
  if (raster)
      raster->draw_lift(lift_x, y);
  y += 2;
  if (y == 0xe0)
    y = 6;
  lift_y[current_lift] = y;
  if (raster)
      raster->draw_lift(lift_x, y);
  current_lift = 1 - current_lift;
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

static void ReduceBonus(void)
{
  int n;
  int flag;

  n = 2;
  do {
      if (raster)
	  raster->draw_bonus(n);
      bonus[n]--;
      flag = ((bonus[n] & 0x80) != 0);
      if (flag) {
	  bonus[n] = 9;
      }
      if (raster)
	  raster->draw_bonus(n);
      n--;
  } while (flag);
  if (bonus[0] + bonus[1] + bonus[2] == 0)
    zero_bonus = 1;
}

int DuckSprite(int n)
{
  int dir;
  int sprite;

  dir = duck[n].dir;
  switch (duck[n].mode) {
  case DUCK_BORED:
      if (dir & DIR_HORIZ) {
	  sprite = (dir == DIR_R) ? 0 : 2;
      } else {
	  sprite = 4;
      }
      break;
  case DUCK_STEP:
      if (dir & DIR_HORIZ) {
	  sprite = (dir == DIR_R) ? 1 : 3;
      } else {
	  sprite = 5;
      }
      break;
  case DUCK_EAT2:
  case DUCK_EAT4:
      sprite = (dir == DIR_R) ? 6 : 8;
      break;
  case DUCK_EAT3:
      sprite = (dir == DIR_R) ? 7 : 9;
      break;
  default:
      abort();
  };
  return sprite;
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
      if (raster)
	  raster->draw_big_duck();
      if (have_big_duck) {
	  tmp = big_duck_x + 4;
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
	  tmp = big_duck_y + big_duck_dy;
	  if (tmp < 0x28)
	    big_duck_dy = -big_duck_dy;
	  tmp = big_duck_x + big_duck_dx;
	  if (tmp < 0 || tmp >= 0x90)
	    big_duck_dx = -big_duck_dx;
      }
      big_duck_x += big_duck_dx;
      big_duck_y += big_duck_dy;
      big_duck_frame ^= 1;
      if (raster)
	  raster->draw_big_duck();
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
	  if (raster)
	      raster->draw_timer(x);
	  timer_ticks[x]--;
	  flag = (timer_ticks[x] & 0x80) != 0;
	  if (flag)
	    timer_ticks[x] = 9;
	  if (raster)
	      raster->draw_timer(x);
	  x--;
      } while (flag);
      tmp = timer_ticks[0] + timer_ticks[1] + timer_ticks[2];
      if (tmp == 0) {
	  abort();
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
  if (raster)
      raster->draw_duck(current_duck);
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
      if (raster)
	  raster->draw_duck(current_duck);
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
  if (raster)
      raster->draw_duck(current_duck);
  return;
}

static void MaybeAddExtraLife(void)
{
  if (extra_life == 0)
    return;
  extra_life = 0;
  DrawLastLife();
  player_data->lives++;
}

static void CollisionDetect(void)
{
    int n;

    /* Little ducks */
    for (n = 0; n < num_ducks; n++) {
	if ((unsigned)((duck[n].x - player_x) + 5) < 0x0b
	    && (unsigned)((duck[n].y - 1) - player_y + 0xe) < 0x1d)
	  is_dead++;
    }
    /* Big duck */
    if (!have_big_duck)
      return;
    if ((unsigned)(big_duck_x + 4 - player_x + 5) >= 0x0b)
      return;
    if ((unsigned)(big_duck_y - 5 - player_y + 0x0e) >= 0x1d)
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
      player_data->lives = 5;
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

void run_game(void)
{
  int tmp;

new_game:
  start_game();
next_player:
  /* "Get Ready" */
next_level:
  SetupLevel();
  if (raster)
    raster->clear_screen();
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
      if (--player_data->lives == 0) {
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
      } while (tmp >= num_players || all_player_data[tmp].lives == 0);
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
