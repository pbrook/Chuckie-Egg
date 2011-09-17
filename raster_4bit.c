/* 4-bit planar sprite rendering routines
   Written by Paul Brook
   Released under the GNU GPL v3.  */

#include "chuckie.h"
#include "raster.h"
#include "spritedata.h"
#include <stdlib.h>
#include <string.h>

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

uint8_t pixels[160 * 256];

static sprite_t *player_sprite;

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

static void DrawTimer(int n)
{
  Do_RenderDigit(0x91 + n * 5, 0xe7, timer_ticks[n]);
}

static void DrawBonus(int n)
{
  Do_RenderDigit(0x66 + n * 5, 0xe7, bonus[n]);
}

static void DrawLives(int player)
{
  int x = 0x1b + 0x22 * player;
  int i;

  for (i = 0; i < 6; i++) {
      Do_RenderDigit(x + 1 + i * 5, 0xf7, all_player_data[player].score[i + 2]);
  }

  i = all_player_data[player].lives;
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

  Do_RenderDigit(0x91, 0xe7, timer_ticks[0]);
  Do_RenderDigit(0x96, 0xe7, 0);
  Do_RenderDigit(0x9b, 0xe7, 0);
}

static void DrawScore(int n, int oldval, int newval)
{
    int x;
    if (n < 2)
	return;
    x = (current_player * 0x22) + 0x12 + n * 5;;
    Do_RenderDigit(x, 0xf7, oldval);
    Do_RenderDigit(x, 0xf7, newval);
}

static void DrawLift(int x, int y)
{
    Do_RenderSprite(x, y, &SPRITE_LIFT, COLOR_LIFT);
}

static void DrawTile(int x, int y, int type)
{
    sprite_t *sprite;
    int color;
    if (type & TILE_LADDER) {
	sprite = &SPRITE_LADDER;
	color = COLOR_LADDER;
    } else if (type & TILE_WALL) {
	sprite = &SPRITE_WALL;
	color = COLOR_WALL;
    } else if (type & TILE_EGG) {
	sprite = &SPRITE_EGG;
	color = COLOR_EGG;
    } else if (type & TILE_GRAIN) {
	sprite = &SPRITE_GRAIN;
	color = COLOR_GRAIN;
    } else {
	abort();
    }
    Do_RenderSprite(x << 3, (y << 3) | 7, sprite, color);
}

static void DrawCage(int is_open)
{
    sprite_t *sprite = have_big_duck ? &SPRITE_CAGE_OPEN : &SPRITE_CAGE_CLOSED;
    Do_RenderSprite(0, 0xdc, sprite, PLANE_YELLOW);
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

static void DrawLife(int n)
{
  int x;

  if (n >= 9)
      return;
  x = (n << 2) + (current_player * 0x22) + 0xd + 0xa;
  Do_RenderSprite(x, 0xee, &SPRITE_HAT, PLANE_YELLOW);
}

static void ClearScreen(void)
{
  memset(pixels, 0, 160*256);
}

static raster_hooks raster_4bit = {
    .erase_player = ErasePlayer,
    .draw_player = DrawPlayer,
    .draw_bonus = DrawBonus,
    .draw_timer = DrawTimer,
    .draw_hud = DrawHUD,
    .draw_score = DrawScore,
    .draw_tile = DrawTile,
    .draw_cage = DrawCage,
    .draw_big_duck = DrawBigDuck,
    .draw_duck = DrawDuck,
    .draw_life = DrawLife,
    .draw_lift = DrawLift,
    .clear_screen = ClearScreen,
};

raster_hooks *raster = &raster_4bit;
