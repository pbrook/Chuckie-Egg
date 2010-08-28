/* Copyright (C) 2009 Paul Brook
   This code is licenced under the GNU GPL v3 */
#include "config.h"
#include "chuckie.h"

#define BUFSIZE 512

#define FREQ 44100

short next_buffer[BUFSIZE];

static int beep_vol[] = {0x3fff, 0x1fff, 0};
static int noise_vol[] =
{
  0x1fff, 0x3fff, 0x3fff, 0x3fff, 0x3fff, 0x3fff, 0x3fff, 0x3fff,
  0x3fff, 0x3fff, 0x3fff, 0x3fff, 0x3fff, 0x3fff, 0x3fff, 0x3fff,
  0x3fff, 0x3fff, 0x3fff, 0x1fff, 0};

static int tone_level[2];
static int *tone_vol[2] = {noise_vol, beep_vol};
static int tone_pos[2];
static int tone_cycles[2];
static int tone_period[2];
static int tone_len[2];

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
sound_start(int note, int channel)
{
  int rate;

  if (channel != 0)
    {
      if (note > 255)
	note = 255;
      else if (note < 0)
	note = 0;
      rate = period_lookup[note];
    }
  else
    {
      switch (note & 3)
	{
	case 0:
	  rate = FREQ / 7000;
	  break;
	case 1:
	  rate = FREQ / 3500;
	  break;
	case 2:
	  rate = FREQ / 1750;
	  break;
	default:
	  rate = 0;
	  break;
	}
    }
  tone_pos[channel] = 0;
  tone_period[channel] = rate;
}

static int
get_noise(void)
{
  static int seed = 0x4000;
  int bit;

  bit = seed & 1;
  seed >>= 1;
  bit ^= (seed & 1);
  seed |= bit << 15;
  return seed & 1;
}

void
mix_buffer (void)
{
  int i;
  int n;
  short val;
  short *p = next_buffer;

  for (i = 0; i < BUFSIZE; i++)
    {
      val = 0;
      for (n = 0; n < 2; n++)
	{
	  if (tone_period[n] == 0)
	    continue;
	  if (tone_level[n])
	    val += tone_vol[n][tone_pos[n]];
	  else
	    val -= tone_vol[n][tone_pos[n]];

	  if (tone_cycles[n] <= 0)
	    {
	      tone_cycles[n] = tone_period[n];
	      if (n == 0)
		tone_level[n] = get_noise();
	      else
		tone_level[n] = !tone_level[n];
	      if (tone_len[n] >= FREQ / 100)
		{
		  tone_len[n] -= FREQ / 100;
		  tone_pos[n]++;
		}
	      if (tone_vol[n][tone_pos[n]] == 0)
		tone_period[n] = 0;
	      tone_cycles[n] = tone_period[n];
	    }
	  else
	    tone_cycles[n]--;
	  tone_len[n]++;
	}
      *(p++) = val;
    }
}

