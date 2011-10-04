/* SDL input handling
   Copyright (C) 2011 Paul Brook
   This code is licenced under the GNU GPL v3 */

#include "chuckie.h"

#include <SDL.h>

SDLKey keys[5][6] = {
    /* right */ {SDLK_PERIOD, SDLK_RIGHT, SDLK_ESCAPE},
    /* left */  {SDLK_COMMA, SDLK_LEFT, SDLK_ESCAPE},
    /* down */  {SDLK_z, SDLK_DOWN, SDLK_ESCAPE},
    /* up */    {SDLK_a, SDLK_UP, SDLK_ESCAPE},
    /* jump */  {SDLK_SPACE, SDLK_HOME, SDLK_END, SDLK_PAGEUP,
		 SDLK_PAGEDOWN, SDLK_ESCAPE}
};

static uint32_t next_time;

float rotate_x;
float rotate_y;
int ortho = 1;

/* Read inputs, and wait until the start of the next frame.  */
void PollKeys(void)
{
  SDL_Event event;
  int i;
  int sym;
  SDLKey *keylist;
  uint32_t now;
  int32_t delta;

  now = SDL_GetTicks();
  if (next_time == 0)
      next_time = now;
  delta = next_time - now;
  skip_frame = (delta < 0);
  next_time += 30;
  while (1) {
      if (skip_frame) {
	  if (!SDL_PollEvent (&event)) {
	      return;
	  }
      } else if (SDL_WaitEvent(&event) == 0) {
	  fprintf(stderr, "SDL_WaitEvent: %s\n", SDL_GetError());
	  exit(1);
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
	  sym = event.key.keysym.sym;
	  switch (sym) {
	  case SDLK_ESCAPE:
	  case SDLK_q:
	      SDL_Quit();
	      exit(0);
	  case SDLK_o:
	      ortho = !ortho;
	      break;
	  case SDLK_f:
	      rotate_x += 0.2;
	      break;
	  case SDLK_h:
	      rotate_x -= 0.2;
	      break;
	  case SDLK_t:
	      rotate_y += 0.2;
	      break;
	  case SDLK_g:
	      rotate_y -= 0.2;
	      break;
	  case SDLK_l:
	      if (event.type == SDL_KEYUP)
		cheat = 1;
	      break;
	  default:
	      for (i = 0; i < 5; i++) {
		  keylist = &keys[i][0];
		  while (*keylist != SDLK_ESCAPE) {
		      if (event.key.keysym.sym == *keylist) {
			  if (event.key.state == SDL_PRESSED)
			      buttons |= 1 << i;
			  else
			      buttons &= ~(1 << i);
		      }
		      keylist++;
		  }
	      }
	      break;
	  }
	  break;
      }
  }
}

