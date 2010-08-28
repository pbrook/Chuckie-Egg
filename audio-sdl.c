#include "config.h"
#include "chuckie.h"
#include "SDL.h"

static int bufferpos;

static void fill_audio(void *arg, Uint8 *stream, int len)
{
  int n = 0;

  while (len)
    {
      if (bufferpos == 0)
	mix_buffer();
      n = (BUFSIZE - bufferpos) * 2;
      if (n > len)
	n = len;
      memcpy (stream, next_buffer + bufferpos, n);
      stream += n;
      len -= n;
      bufferpos = 0;
    }
  if (n != BUFSIZE * 2)
    bufferpos = n / 2;
}

void init_sound(void)
{
  int err;
  SDL_AudioSpec desired, obtained;

  err = SDL_InitSubSystem(SDL_INIT_AUDIO);
  if (err)
    return;
  desired.callback = fill_audio;
  desired.userdata = NULL;
  desired.freq = FREQ / 2; /* ??? */
  desired.format = AUDIO_S16SYS;
  desired.channels = 2;
  desired.samples = 4096;

  if (SDL_OpenAudio(&desired, &obtained))
    {
      fprintf(stderr, "Audio init failed\n");
      return;
    }
  printf("Got %d\n", obtained.freq);
  SDL_PauseAudio(0);
}
