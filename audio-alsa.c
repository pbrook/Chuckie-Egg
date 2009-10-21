/* Copyright (C) 2009 Paul Brook
   This code is licenced under the GNU GPL v3 */
#include "config.h"
#include <pthread.h>
#include <alsa/asoundlib.h>

static char dev_name[] = "default";
#define DEV_NAME dev_name

#define BUFSIZE 512

#define FREQ 44100

static snd_pcm_t *playback_handle;
static short next_buffer[BUFSIZE];

static int tone_flip;
static volatile int tone_vol;
static int tone_cycles;
static int tone_period;
static int tone_len;

static const int period_lookup[256] = 
{
178, 176, 173, 171, 168, 166, 163, 161,
159, 156, 154, 152, 150, 148, 145, 143,
141, 139, 137, 135, 133, 131, 129, 128,
126, 124, 122, 120, 119, 117, 115, 114,
112, 110, 109, 107, 106, 104, 103, 101,
100, 98, 97, 95, 94, 93, 91, 90,
89, 88, 86, 85, 84, 83, 81, 80,
79, 78, 77, 76, 75, 74, 72, 71,
70, 69, 68, 67, 66, 65, 64, 64,
63, 62, 61, 60, 59, 58, 57, 57,
56, 55, 54, 53, 53, 52, 51, 50,
50, 49, 48, 47, 47, 46, 45, 45,
44, 44, 43, 42, 42, 41, 40, 40,
39, 39, 38, 38, 37, 37, 36, 35,
35, 34, 34, 33, 33, 32, 32, 32,
31, 31, 30, 30, 29, 29, 28, 28,
28, 27, 27, 26, 26, 26, 25, 25,
25, 24, 24, 23, 23, 23, 22, 22,
22, 22, 21, 21, 21, 20, 20, 20,
19, 19, 19, 19, 18, 18, 18, 17,
17, 17, 17, 16, 16, 16, 16, 16,
15, 15, 15, 15, 14, 14, 14, 14,
14, 13, 13, 13, 13, 13, 12, 12,
12, 12, 12, 12, 11, 11, 11, 11,
11, 11, 10, 10, 10, 10, 10, 10,
9, 9, 9, 9, 9, 9, 9, 8,
8, 8, 8, 8, 8, 8, 8, 8,
7, 7, 7, 7, 7, 7, 7, 7,
7, 6, 6, 6, 6, 6, 6, 6,
6, 6, 6, 6, 5, 5, 5, 5,
5, 5, 5, 5, 5, 5, 5, 5,
4, 4, 4, 4, 4, 4, 4, 4
};

void
sound_start(int note)
{
  if (note > 255)
    note = 255;
  else if (note < 0)
    note = 0;
  tone_cycles = tone_period = period_lookup[note];
  tone_len = FREQ / 50;
  tone_vol = 0x3fff;
}

static void
mix_buffer (void)
{
  int i;
  short val;
  short *p = next_buffer;

  if (tone_vol == 0)
    {
      memset (p, 0, 2 * BUFSIZE);
      return;
    }
  for (i = 0; i < BUFSIZE; i++)
    {
      val = tone_vol;
      if (tone_flip)
	val = -val;
      *(p++) = val;
      if (tone_cycles <= 0)
	{
	  tone_cycles = tone_period;
	  tone_flip = !tone_flip;
	  if (tone_len <= 0)
	    tone_vol = 0;
	  else if (tone_len < (FREQ / 100))
	    tone_vol = 0x1fff;
	}
      tone_cycles--;
      tone_len--;
    }
}

#if 0
static void
cs(void)
{
  switch (snd_pcm_state(playback_handle))
    {
    case SND_PCM_STATE_SETUP:
      g_print("SETUP\n");
      break;
    case SND_PCM_STATE_PREPARED:
      g_print("PREPARED\n");
      break;
    case SND_PCM_STATE_RUNNING:
      g_print("RUNNING\n");
      break;
    case SND_PCM_STATE_XRUN:
      g_print("XRUN\n");
      break;
    case SND_PCM_STATE_OPEN:
      g_print("OPEN\n");
      break;
    case SND_PCM_STATE_SUSPENDED:
      g_print("SUSPENDED\n");
      break;
    case SND_PCM_STATE_PAUSED:
      g_print("PAUSED\n");
      break;
    default:
      g_print("Unknown\n");
      break;
    }
}
#endif

static void
pcm_callback (snd_async_handler_t *handler)
{
  int avail;
  int ret;

  avail = snd_pcm_avail_update(playback_handle);
  while (avail >= BUFSIZE)
    {
      mix_buffer();
      ret = snd_pcm_writei (playback_handle, next_buffer, BUFSIZE);
      if (ret < 0)
	{
	  fprintf(stderr, "xrun\n");
	  snd_pcm_prepare (playback_handle);
	}
      avail -= BUFSIZE;
    }
}

static void
init_pcm(void)
{
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    unsigned int rate = FREQ;
    int rc;
            
    if ((rc = snd_pcm_open (&playback_handle, DEV_NAME, SND_PCM_STREAM_PLAYBACK, SND_PCM_ASYNC)) < 0) {
        fprintf (stderr, "cannot open audio device %s (%d)\n", DEV_NAME, rc);
        exit (1);
    }
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(playback_handle, hw_params);
    snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, NULL);
    snd_pcm_hw_params_set_channels(playback_handle, hw_params, 1);
#if 1
    snd_pcm_hw_params_set_periods(playback_handle, hw_params, 3, 0);
    snd_pcm_hw_params_set_period_size(playback_handle, hw_params, BUFSIZE, 0);
#endif
    snd_pcm_hw_params(playback_handle, hw_params);
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(playback_handle, sw_params);
    snd_pcm_sw_params_set_avail_min(playback_handle, sw_params, BUFSIZE);
    snd_pcm_sw_params_set_start_threshold(playback_handle, sw_params, 0);
    snd_pcm_sw_params(playback_handle, sw_params);

    snd_pcm_prepare(playback_handle);
}

static void *
sound_thread (void *arg)
{
  init_pcm ();
  while (1)
    {
      int err;

      err = snd_pcm_wait (playback_handle, 10);
      if (err < 0) {
	fprintf(stderr, "Sound Error %d\n", err);
      }
      pcm_callback(NULL);
    }
}

void
init_sound(void)
{
  pthread_t th;

  pthread_create (&th, NULL, sound_thread, NULL);
}
