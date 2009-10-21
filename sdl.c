#include "chuckie.h"

#include "SDL.h"

SDL_Surface *sdlscreen;

SDLKey keys[8] = {
    SDLK_PERIOD,
    SDLK_COMMA,
    SDLK_z,
    SDLK_a,
    SDLK_SPACE
};

void die(const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  fprintf(stderr, "Error: ");
  vfprintf(stderr, msg, va);
  va_end(va);
  exit(1);
}

/* Read inputs, and wait until the start of the next frame.  */
void PollKeys(void)
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

void RenderFrame(void)
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

  init_sound();

  SDL_AddTimer(30, do_timer, NULL);

  run_game();
}

