/* Copyright (C) 2009 Paul Brook
   This code is licenced under the GNU GPL v3 */
#include "config.h"
#include "chuckie.h"
#include <pthread.h>
#include <alsa/asoundlib.h>

static char dev_name[] = "default";
#define DEV_NAME dev_name

static snd_pcm_t *playback_handle;

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

static int
init_pcm(void)
{
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    unsigned int rate = FREQ;
    int rc;
            
    if ((rc = snd_pcm_open (&playback_handle, DEV_NAME, SND_PCM_STREAM_PLAYBACK, SND_PCM_ASYNC)) < 0) {
        fprintf (stderr, "cannot open audio device %s (%d)\n", DEV_NAME, rc);
	return 1;
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
    return 0;
}

static void *
sound_thread (void *arg)
{
  if (init_pcm ())
    return NULL;

  while (1)
    {
      int err;

      err = snd_pcm_wait (playback_handle, 10);
      if (err == -EPIPE) {
	snd_pcm_prepare(playback_handle);
      } else if (err < 0 && err != -EPIPE) {
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
