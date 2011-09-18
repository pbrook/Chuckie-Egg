/* OpenGL setup and rendering code.
   Copyright (C) 2011 Paul Brook
   This code is licenced under the GNU GPL v3 */
#include "chuckie.h"
#include "raster.h"
#include "spritedata.h"

#include <SDL.h>
#include <SDL_opengl.h>

static int fullscreen = 0;
static int scale = 2;

raster_hooks *raster = NULL;

static SDL_Surface *sdlscreen;

void die(const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  fprintf(stderr, "Error: ");
  vfprintf(stderr, msg, va);
  va_end(va);
  exit(1);
}

#define YELLOW {0xff, 0xff, 0} /* Player, lift, big duck */
#define BLUE {0, 0xff, 0xff} /* Small duck */
#define GREEN {0, 0xff, 0} /* Floor */
#define PURPLE {0xff, 0, 0xff} /* Ladder */

static Uint32 do_timer(Uint32 interval, void *param)
{
  SDL_Event event;
  event.type = SDL_USEREVENT;
  SDL_PushEvent(&event);
  return interval;
}

typedef struct {
    GLfloat w;
    GLfloat h;
    GLfloat x1;
    GLfloat x2;
    GLfloat y1;
    GLfloat y2;
    SDL_Color color;
} *gltex;

#define TEX_SIZE 128
static GLubyte tex_buffer[TEX_SIZE * TEX_SIZE * 4];
static GLuint tex_handle;
static int tex_x;
static int tex_y;
static int tex_h;

static gltex LoadTexture(sprite_t *sprite, SDL_Color color)
{
    gltex tex;
    GLubyte *dest;
    const uint8_t *src;
    uint8_t mask = 0;
    int i;
    int j;
    int stride;

    tex = malloc(sizeof(*tex));
    if (tex_x + sprite->x > TEX_SIZE) {
	tex_y += tex_h;
	tex_h = 0;
	tex_x = 0;
    }
    if (sprite->y > tex_h)
	tex_h = sprite->y;
    if (tex_y + tex_h > TEX_SIZE)
	abort();
    stride = (TEX_SIZE - sprite->x) * 4;
    dest = &tex_buffer[(tex_x + tex_y * TEX_SIZE) * 4];
    src = sprite->data;
    for (j = 0; j < sprite->y; j++) {
	for (i = 0; i < sprite->x; i++) {
	    if ((i & 7) == 0)
		mask = *(src++);
	    *(dest++) = color.r;
	    *(dest++) = color.g;
	    *(dest++) = color.b;
	    if (mask & 0x80) {
		*(dest++) = 255;
	    } else {
		*(dest++) = 0;
	    }
	    mask <<= 1;
	}
	dest += stride;
    }
    tex->x1 = tex_x / (float)TEX_SIZE;
    tex->x2 = tex->x1 + sprite->x / (float)TEX_SIZE;
    tex->y1 = tex_y / (float)TEX_SIZE;
    tex->y2 = tex->y1 + sprite->y / (float)TEX_SIZE;
    tex->color = color;
    tex->w = sprite->x;
    tex->h = sprite->y;
    tex_x += sprite->x;
    return tex;
}

static void FinishTextures(void)
{
    glGenTextures(1, &tex_handle);
    glBindTexture(GL_TEXTURE_2D, tex_handle);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		 TEX_SIZE, TEX_SIZE, 0,
		 GL_RGBA, GL_UNSIGNED_BYTE, &tex_buffer);
    if (glGetError() != GL_NO_ERROR) {
	die("Failed to load texture\n");
    }
}

static SDL_Color sdl_yellow = YELLOW;
static SDL_Color sdl_purple = PURPLE;
static SDL_Color sdl_green = GREEN;
static SDL_Color sdl_blue = BLUE;
static SDL_Color sdl_black = {0, 0, 0};

static gltex gltex_wall;
static gltex gltex_ladder;
static gltex gltex_egg;
static gltex gltex_grain;
static gltex gltex_lift;

static gltex gltex_duck[10];
static gltex gltex_big_duck_l1;
static gltex gltex_big_duck_l2;
static gltex gltex_big_duck_r1;
static gltex gltex_big_duck_r2;
static gltex gltex_cage_open;
static gltex gltex_cage_closed;

static gltex gltex_player_r[4];
static gltex gltex_player_l[4];
static gltex gltex_player_up[4];

static gltex gltex_score;
static gltex gltex_blank;
static gltex gltex_player;
static gltex gltex_level;
static gltex gltex_bonus;
static gltex gltex_time;
static gltex gltex_hat;
static gltex gltex_digit[10];

static void LoadTextures(void)
{
    int i;

    gltex_cage_open = LoadTexture(&SPRITE_CAGE_OPEN, sdl_yellow);
    gltex_cage_closed = LoadTexture(&SPRITE_CAGE_CLOSED, sdl_yellow);
    gltex_big_duck_l1 = LoadTexture(&SPRITE_BIGDUCK_L1, sdl_yellow);
    gltex_big_duck_l2 = LoadTexture(&SPRITE_BIGDUCK_L2, sdl_yellow);
    gltex_big_duck_r1 = LoadTexture(&SPRITE_BIGDUCK_R1, sdl_yellow);
    gltex_big_duck_r2 = LoadTexture(&SPRITE_BIGDUCK_R2, sdl_yellow);
    for (i = 0; i < 10; i++)
	gltex_duck[i] = LoadTexture(sprite_duck[i], sdl_blue);
    for (i = 0; i < 4; i++) {
	gltex_player_r[i] = LoadTexture(sprite_player_r[i], sdl_yellow);
	gltex_player_l[i] = LoadTexture(sprite_player_l[i], sdl_yellow);
	gltex_player_up[i] = LoadTexture(sprite_player_up[i], sdl_yellow);
    }

    gltex_wall = LoadTexture(&SPRITE_WALL, sdl_green);
    gltex_ladder = LoadTexture(&SPRITE_LADDER, sdl_purple);
    gltex_egg = LoadTexture(&SPRITE_EGG, sdl_yellow);
    gltex_grain = LoadTexture(&SPRITE_GRAIN, sdl_purple);

    gltex_score = LoadTexture(&SPRITE_SCORE, sdl_purple);
    gltex_blank = LoadTexture(&SPRITE_BLANK, sdl_purple);
    gltex_player = LoadTexture(&SPRITE_PLAYER, sdl_purple);
    gltex_level = LoadTexture(&SPRITE_LEVEL, sdl_purple);
    gltex_bonus = LoadTexture(&SPRITE_BONUS, sdl_purple);
    gltex_time = LoadTexture(&SPRITE_TIME, sdl_purple);
    for (i = 0; i < 10; i++)
	gltex_digit[i] = LoadTexture(&sprite_digit[i], sdl_black);
    gltex_lift = LoadTexture(&SPRITE_LIFT, sdl_yellow);
    gltex_hat = LoadTexture(&SPRITE_HAT, sdl_yellow);
    FinishTextures();
}

static void RenderSprite(gltex t, GLfloat x, GLfloat y)
{
    GLfloat w = t->w;
    GLfloat h = t->h;
    //glBindTexture(GL_TEXTURE_2D, t->handle);
    glBegin(GL_QUADS);
    glTexCoord2f(t->x1, t->y1);
    glVertex2f(x, y);
    glTexCoord2f(t->x1, t->y2);
    glVertex2f(x, y - h);
    glTexCoord2f(t->x2, t->y2);
    glVertex2f(x + w, y - h);
    glTexCoord2f(t->x2, t->y1);
    glVertex2f(x + w, y);
    glEnd();
}

static void RenderDigit(int x, int y, int n)
{
    RenderSprite(gltex_digit[n], x, y);
}

static void RenderPlayerHUD(int player)
{
    int x = player * 0x22 + 0x1b;
    int n;

    RenderSprite(gltex_blank, x, 0xf0);

    for (n = 0; n < 6; n++) {
	RenderDigit(x + 1 + n * 5, 0xef,
		    all_player_data[player].score[n + 2]);
    }

    n = all_player_data[player].lives;
    if (n > 8)
      n = 8;
    while (n--) {
	RenderSprite(gltex_hat, x, 0xe7);
	x += 4;
    }
}

void RenderFrame(void)
{
    int x;
    int y;
    int n;
    int sprite;
    int type;
    gltex tex;
    gltex *ps;

    glClear(GL_COLOR_BUFFER_BIT);

    /* HUD  */
    RenderSprite(gltex_score, 0, 0xf0);
    for (n = 0; n < num_players; n++) {
	RenderPlayerHUD(n);
    }

    y = 0xe3;
    RenderSprite(gltex_player, 0, y + 1);
    RenderDigit(0x1b, y, current_player + 1);

    RenderSprite(gltex_level, 0x24, y + 1);
    n = current_level + 1;
    RenderDigit(0x45, y, n % 10);
    n /= 10;
    RenderDigit(0x40, y, n % 10);
    if (n > 10)
	RenderDigit(0x3b, y, n % 10);

    RenderSprite(gltex_bonus, 0x4e, y + 1);
    RenderDigit(0x66, y, bonus[0]);
    RenderDigit(0x66, y, bonus[0]);
    RenderDigit(0x6b, y, bonus[1]);
    RenderDigit(0x70, y, bonus[2]);
    RenderDigit(0x75, y, 0);

    RenderSprite(gltex_time, 0x7e, y + 1);
    RenderDigit(0x91, y, timer_ticks[0]);
    RenderDigit(0x96, y, timer_ticks[1]);
    RenderDigit(0x9b, y, timer_ticks[2]);

    /* Background.  */
    for (x = 0; x < 20; x++) {
	for (y = 0; y < 25; y++) {
	    type = levelmap[y * 20 + x];
	    if (type & TILE_LADDER) {
		tex = gltex_ladder;
	    } else if (type & TILE_WALL) {
		tex = gltex_wall;
	    } else if (type & TILE_EGG) {
		tex = gltex_egg;
	    } else if (type & TILE_GRAIN) {
		tex = gltex_grain;
	    } else {
		continue;
	    }
	    RenderSprite(tex, x << 3, (y << 3) | 7);
	}
    }

    /* Ducks.  */
    for (n = 0; n < num_ducks; n++) {
	x = duck[n].x;
	sprite = DuckSprite(n);
	if (sprite >= 8)
	    x -= 8;

	RenderSprite(gltex_duck[sprite], x, duck[n].y);
    }

    /* Player.  */
    if (player_face == 0) {
	ps = gltex_player_up;
	n = (player_y >> 1) & 3;
    } else {
	if (player_face < 0)
	  ps = gltex_player_l;
	else
	  ps = gltex_player_r;
	n = (player_x >> 1) & 3;
    }
    if (player_mode != PLAYER_CLIMB) {
	if (move_x == 0)
	  n = 0;
    } else {
	if (move_y == 0)
	  n = 0;
    }
    RenderSprite(ps[n], player_x, player_y);

    /* Lift.  */
    if (have_lift) {
	for (n = 0; n < 2; n++) {
	    RenderSprite(gltex_lift, lift_x, lift_y[n]);
	}
    }

    /* Big duck.  */
    if (big_duck_dir) {
	tex = big_duck_frame ? gltex_big_duck_l2 : gltex_big_duck_l1;
    } else {
	tex = big_duck_frame ? gltex_big_duck_r2 : gltex_big_duck_r1;
    }
    RenderSprite(tex, big_duck_x, big_duck_y);
    if (have_big_duck) {
	tex = gltex_cage_open;
    } else {
	tex = gltex_cage_closed;
    }
    RenderSprite(tex, 0, 0xdc);

    SDL_GL_SwapBuffers();
    if (glGetError() != GL_NO_ERROR) {
	die("Error rendering frame\n");
    }
}

static void parse_args(int argc, const char *argv[])
{
    const char *p;
    int i;

    for (i = 1; i < argc; i++) {
	p = argv[i];
	if (*p == '-')
	  p++;
	if (*p >= '1' && *p <= '9')
	  scale = *p - '0';
	else if (*p == 'f')
	  fullscreen = 1;
	else if (*p == 'h') {
	    printf("Usage: chuckie -123456789f\n");
	    exit(0);
	}
    }
}

int main(int argc, const char *argv[])
{
  parse_args (argc, argv);
  int flags;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1) {
      die("SDL_Init: %s\n", SDL_GetError());
  }

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  flags = SDL_OPENGL;
  if (fullscreen)
    flags |= SDL_FULLSCREEN;

  sdlscreen = SDL_SetVideoMode(scale * 320, scale * 240, 32, flags);
  if (!sdlscreen) {
      SDL_Quit();
      fprintf(stderr, "SDL_SetVideoMode failed\n");
      return 0;
  }

  SDL_ShowCursor(SDL_DISABLE);

  init_sound();

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  glClearColor(0, 0, 0, 0);
  LoadTextures();

  glViewport(0, 0, scale * 320, scale * 240);
  glMatrixMode(GL_PROJECTION);
  glOrtho(0, 160, 0, 240, 1, -1);
  SDL_AddTimer(30, do_timer, NULL);

  run_game();

  return 0;
}
