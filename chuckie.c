#include "config.h"

#include <stdint.h>
#include <stdarg.h>

#include "SDL.h"

#define NUM_PLAYERS 1

#include "ram.c"
#define LD2(addr) (RAM[addr] | (RAM[(addr) + 1] << 8))

SDL_Surface *sdlscreen;

void die(const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  fprintf(stderr, "Error: ");
  vfprintf(stderr, msg, va);
  va_end(va);
  exit(1);
}

uint8_t pixels[160 * 256];

static void Do_RenderSprite(int x, int y, int sprite, int color)
{
    uint8_t *src;
    uint8_t srcbits;
    int bits;
    int sx;
    int sy;
    int i;
    uint8_t *dest;

    y = y ^ 0xff;
    src = &RAM[0x1100 + (sprite << 2)];
    sx = src[0];
    sy = src[1];
    src = &RAM[src[2] + (src[3] << 8)];
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

static void DrawLives(int player)
{
  int tmp;
  RAM[0x8b] = 0x22 * player + 0x1b;
  RAM[0x8d] = (player << 6) + 2;
  RAM[0x8c] = RAM[0x8b] + 1;
  RAM[0x8e] = 6;
label_1e16:
  Do_RenderSprite(RAM[0x8c], 0xf7, RAM[0x0500 + RAM[0x8d]] + 0x1f, 2); /* 0x1e1f */
  RAM[0x8c] += 5;
  RAM[0x8d]++;
  RAM[0x8e]--;
  if (RAM[0x8e] != 0) /* 0x1e2d */
    goto label_1e16;
  tmp = RAM[player + 0x20];
  if (tmp == 0)
  if (tmp == 0)  /* 0x1e37 */
    goto label_1e62;
  if (tmp > 8) { /* 0x1e3f */
      tmp = 8;
  }
  RAM[0x8e] = tmp;
  RAM[0x8c] = RAM[0x8b];
label_1e45:
  Do_RenderSprite(RAM[0x8c], 0xee, 0x2f, 4);
  RAM[0x8e]--;
  if (RAM[0x8e] == 0) /* 0x1e56 */
    goto label_1e62;
  RAM[0x8c] += 4;
  goto label_1e45;
label_1e62:
  return;
}

#define G_Player RAM[0x5d]

static void Do_RenderDigit(int x, int y, int n)
{
    Do_RenderSprite(x, y, n + 0x1f, 2);
}

/* DrawStatics? */
static void do_1cc3(void)
{
  int tmp;

  Do_RenderSprite(0, 0xf8, 0x29, 2); /* 0x1cd3 */
  tmp = G_Player * 0x22 + 0x1b;
  Do_RenderSprite(tmp, 0xf8, 0x2a, 2); /* 0x1cee */
  for (tmp = 0; tmp < RAM[0x5e]; tmp++) {
      DrawLives(tmp);
  }
  Do_RenderSprite(0, 0xe8, 0x2b, 2); /* 0x1d10 */
  Do_RenderSprite(0x1b, 0xe7, G_Player + 0x20, 2); /* 0x1d22 */
  Do_RenderSprite(0x24, 0xe8, 0x2c, 2); /* 0x1d31 */

  tmp = RAM[0x50] + 1;
  RAM[0x8d] = tmp % 10;
  tmp /= 10;
  RAM[0x8c] = tmp % 10;
  tmp /= 10;
  RAM[0x8b] = tmp;
  if (tmp) {
      Do_RenderDigit(0x3b, 0xe7, tmp);
  }
  Do_RenderDigit(0x40, 0xe7, RAM[0x8c]);
  Do_RenderDigit(0x45, 0xe7, RAM[0x8d]);

  Do_RenderSprite(0x4e, 0xe8, 0x2d, 2); /* 0x1d8b */

  Do_RenderDigit(0x66, 0xe7, RAM[0x3A]); /* 0x1d94 */
  Do_RenderDigit(0x6b, 0xe7, RAM[0x3B]);
  Do_RenderDigit(0x70, 0xe7, RAM[0x3C]);
  Do_RenderDigit(0x75, 0xe7, 0); /* 0x1daf */
  Do_RenderSprite(0x7e, 0xe8, 0x2e, 2); /* 0x1dbe */

  tmp = RAM[0x4d] >> 1;
  if (tmp > 8) tmp = 8;
  tmp ^= 0xff;
  tmp = (tmp + 10) & 0xff;
  RAM[0x1d] = tmp;
  Do_RenderDigit(0x91, 0xe7, tmp); /* 0x1dd5 */
  RAM[0x1e] = 0;
  Do_RenderDigit(0x96, 0xe7, 0); /* 0x1de0 */
  RAM[0x1f] = 0;
  Do_RenderDigit(0x9b, 0xe7, 0); /* 0x1deb */
}

/* WriteScreen? */
static void do_23e0(int x, int y, int a)
{
  int offset;
  offset = x + y * 20;
  RAM[0x0600 + offset] = a;
}

/* 0x23c8 */
/* Duck code generates out of bounds reads.  */
static int Do_ReadMap(uint8_t x, uint8_t y)
{
  if (y >= 0x19 || x >= 0x14) {
      return 0;
  }
  return RAM[0x0600 + x + y * 20];
}

/* 0x23e0 and 0x1a0c */
static void Do_InitTile(int x, int y, int sprite, int n, int color)
{
    int offset;
    offset = y * 20 + x;
    RAM[0x0600 + offset] = n;
    Do_RenderSprite(x << 3, (y << 3) | 7, sprite, color);
}

/* InitLevel? */
static void do_1b38(void)
{
  int addr;
  int i;
  int offset;
  int tmp;
  int x;
  int y;

  do_1cc3();

  /* ??? I think X and Y are backwards here :-( */
  addr = LD2(0x0cc0 + (RAM[0x5c] << 1));
  RAM[0x51] = addr & 0xff;
  RAM[0x52] = addr >> 8;
  for (i = 0; i < 5; i++) {
      RAM[0x53 + i] = RAM[addr + i];
  }
  offset = 5;
  /* 0x1b64 */
  for (i = 0; i < 0x200; i++)
    RAM[0x0600 + i] = 0;

  /* 0x1b70 */
  RAM[0x8a] = RAM[0x53];
label_1b7a:
  y = RAM[addr + offset++];
  x = RAM[addr + offset++];
  i = RAM[addr + offset++] - x;

label_1b90:
  Do_InitTile(x, y, 1, 1, 3); /* 0x1b96 */
  x++;
  i--;
  if (i >= 0) /* 0x1ba4 */
    goto label_1b90;
  RAM[0x8a]--;
  if (RAM[0x8a] != 0) /* 0x1ba6 */
    goto label_1b7a;

  RAM[0x8a] = RAM[0x54];
label_1bb0:
  x = RAM[addr + offset++];
  y = RAM[addr + offset++];
  i = RAM[addr + offset++] - y;

label_1bc6:
  tmp = RAM[0x0600 + x + y * 20];
  if (tmp) { /* 0x1bcd */
    Do_RenderSprite(x << 3, (y << 3) | 7, tmp, 3);
  }
  Do_InitTile(x, y, 2, tmp | 2, 2); /* 0x1be4 */
  y++;
  i--;
  if (i >= 0) /* 0x1bf4 */
    goto label_1bc6;
  RAM[0x8a]--;
  if (RAM[0x8a] != 0) /* 0x1bf8 */
    goto label_1bb0;

  if (RAM[0x55]) { /* 0x1bfc */
      int tmp;
      tmp = RAM[addr + offset++];
      tmp <<= 3;
      RAM[0x58] = tmp;
  }

  /* 0c1ca0 */
  RAM[0x8a] = 0;
  RAM[0x39] = 0;
  RAM[0x88] = RAM[0x4e];
label_1c18:
  RAM[0x8b] = RAM[addr + offset++];
  RAM[0x8c] = RAM[addr + offset++];
  tmp = RAM[0x0510 + RAM[0x88]];
  if (tmp == 0) {
      tmp = (RAM[0x8a] << 4) + 4;
      Do_InitTile(RAM[0x8b], RAM[0x8c], 3, tmp, 1);
      RAM[0x39]++;
  }
  RAM[0x88]++;
  RAM[0x8a]++;
  if (RAM[0x8a] < 0xc) /* 0x1c4f */
    goto label_1c18;

  RAM[0x8a] = 0;
  RAM[0x88] = RAM[0x4e];

label_1c5d:
  RAM[0x8b] = RAM[addr + offset++];
  RAM[0x8c] = RAM[addr + offset++];
  tmp = RAM[0x0520 + RAM[0x89]];
  if (tmp == 0) { /* 0x1c70 */
      tmp = (RAM[0x8a] << 4) + 8;
      Do_InitTile(RAM[0x8b], RAM[0x8c], 4, tmp, 2);
  }
  RAM[0x88]++;
  RAM[0x8a]++;
  if (RAM[0x8a] < RAM[0x56]) /* 0x1c92 */
    goto label_1c5d;

  /* 0x1c94 */
  Do_RenderSprite(0, 0xdc, RAM[0x35] ? 0x14 : 0x13, 4); /* 0x1caa */

  /* 0x1cad */
  for (i = 0; i < 5 ; i++) { /* 0x1cc0 */
      RAM[0x040a + i] = RAM[addr + offset++];
      RAM[0x040f + i] = RAM[addr + offset++];
  }
}

/* DrawPlayer */
static void do_2336(int n)
{
  Do_RenderSprite(RAM[0x30], RAM[0x31], n + 0x0f, 4);
}

/* DrawDuck */
static void do_234b(int n)
{
  int sprite;
  int x;
  int y;
  RAM[0x7F] = 0x80;

  sprite = RAM[0x0419 + n] + 0x15;
  x = RAM[0x0400 + n];
  y = RAM[0x0405 + n];
  if (sprite >= 0x1d) /* 0x2367 */
    x -= 8;
  Do_RenderSprite(x, y, sprite, 8); /* 0x2370 */
}

/* DrawBigBird? */
static void do_2324(int sprite)
{
  Do_RenderSprite(RAM[0x40], RAM[0x41], sprite, 4);
}

static void do_2f5a(void)
{
  int tmp;
  tmp = RAM[0x20 + RAM[0x5d]];
  if (tmp >= 9) /* 0x2f64 */
      return;
  tmp = (tmp << 2) + RAM[0x3f] + 0xa;
  Do_RenderSprite(tmp, 0xee, 0x2f, 4);
}

static void do_2e92(void)
{
  int i;
  if (RAM[0x55]) { /* 0x2e94 */
      RAM[0x59] = 8;
      RAM[0x5A] = 0x5a;
      RAM[0x5B] = 0;
      Do_RenderSprite(RAM[0x58], RAM[0x59], 5, 1); /* 0x2eb2 */
      Do_RenderSprite(RAM[0x58], RAM[0x5a], 5, 1); /* 0x2ec1 */
  }
  RAM[0x30] = 4;
  RAM[0x31] = 0xcc;
  RAM[0x32] = RAM[0x33] = RAM[0x34] = 0;
  do_2336(0); /* 0x2ed4 */
  if (RAM[0x4D] == 1) { /* 0x2edf */
      RAM[0x57] = 0;
  }
  if (RAM[0x4D] == 3) { /* 0x2ee7 */
      RAM[0x57] = 5;
  }
  i = -1;
label_2eed:
  i++;
  if (i < RAM[0x57]) { /* 0x2ef3 */
      RAM[0x0400 + i] = RAM[0x040a + i] << 3;
      RAM[0x0405 + i] = (RAM[0x040f + i] << 3) + 0x14;
      RAM[0x0414 + i] = 0;
      RAM[0x0419 + i] = 0;
      RAM[0x041e + i] = 2;
      do_234b(i);
      goto label_2eed; /* 0x2f1a */
  }
  /* 0x2f1d */
  /* Delay(3) */
  RAM[0x40] = 0x3c;
  RAM[0x41] = 0x20;
  RAM[0x48] = 6;
  do_2324(6);
  RAM[0x42] = 7;
  RAM[0x43] = 2;
  RAM[0x44] = 7;
  RAM[0x45] = 0;
  RAM[0x49] = 0;
  RAM[0x4c] = 1;
  do_2f5a(); /* 0x2f45 */
}

/* MoveSideways */
static int do_2276(void)
{
  int tmp, x, y;
  tmp = RAM[0x46];
  if ((tmp & 0x80) != 0) /* 0x2278 */
    goto label_227e;
  if (tmp != 0)
    goto label_22bd;
  return 0;
label_227e:
  if (RAM[0x40] == 0) /* 0x2282 */
    goto label_22fc;
  if (RAM[0x44] >= 2) /* 0x2288 */
    goto label_22fa;
  if (RAM[0x47] == 2) /* 0x228e */
    goto label_22fa;
  x = RAM[0x42] - 1;
  y = RAM[0x43];
  tmp = (uint8_t)(RAM[0x45] - RAM[0x47]);
  if (tmp < 8) /* 0x229c */
    goto label_22a5;
  if (((tmp - 8) & 0x80) == 0)
    goto label_22a4;
  y--;
  goto label_22a5; /* 0x22a1 */
label_22a4:
  y++;
label_22a5:
  tmp = Do_ReadMap(x, y); /* 0x22a5 */
  if (tmp == 1) /* 0x22aa */
    goto label_22fc;
  if ((RAM[0x47] & 0x80) == 0) /* 0x22ae */
    goto label_22fa;
  x = RAM[0x42] - 1;
  y++;
  tmp = Do_ReadMap(x, y); /* 0x22b4 */
  if (tmp == 1)
    goto label_22fc;
  else
    goto label_22fa;
label_22bd:
  tmp = RAM[0x40];
  if (tmp >= 0x98) /* 0x22c1 */
    goto label_22fc;
  tmp = RAM[0x44];
  if (tmp < 5) /* 0x22c7 */
    goto label_22fa;
  if (RAM[0x47] == 2) /* 0x22cd */
    goto label_22fa;
  x = RAM[0x42] + 1;
  y = RAM[0x43];
  tmp = (uint8_t)(RAM[0x45] + RAM[0x47]);
  if (tmp < 8) /* 0x22db */
    goto label_22e4;
  if (((tmp - 8) & 0x80) == 0) /* 0x22dd */
    goto label_22e3;
  y--;
  goto label_22e4; /* 0x22e0 */
label_22e3:
  y++;
label_22e4:
  tmp = Do_ReadMap(x, y);
  if (tmp == 1)
    goto label_22fc;
  if ((RAM[0x47] & 0x80) == 0) /* 0x22ed */
    goto label_22fa;
  x = RAM[0x42] + 1;
  y++;
  tmp = Do_ReadMap(x, y);
  if (tmp == 1)
    goto label_22fc;
label_22fa:
  return 0;
label_22fc:
  return 1;
}

static void do_22fe(int x, int y)
{
  Do_InitTile(x, y, 3, 0, 1);
}

static void do_2311(int x, int y)
{
  Do_InitTile(x, y, 4, 0, 2);
}

/* AddScore2 */
static int do_1ad8(int x, int y, int tmp)
{
  if (x < 2) /* 0x1ada */
    return x;
  RAM[0x8c] = tmp;
  RAM[0x8d] = x;
  RAM[0x8e] = y;
  tmp = RAM[0x3f];
label_1ae6:
  tmp += 5;
  x--;
  if ((x & 0x80) == 0) /* 0x1ae9 */
    goto label_1ae6;
  RAM[0x8b] = tmp;
  x = tmp;
  Do_RenderDigit(x, 0xf7, RAM[0x8e]); /* 0x1af2 */
  Do_RenderDigit(x, 0xf7, RAM[0x8c]); /* 0x1afb */
  return x;
}

/* AddScore */
static void do_1ab5(int x, int a)
{
  int tmp;
  int y;
  tmp = a;
label_1ab5:
  y = RAM[0x28 + x];
  tmp += y;
  if (x == 3)
    RAM[0x3e]++;
  if (tmp < 0x0a) /* 0x1ac6 */
    goto label_1ad6;
  tmp -= 0x0a;
  RAM[0x28 + x] = tmp;
  do_1ad8(x, y, tmp); /* 0x1acd */
  tmp = 1;
  x--;
  if ((x & 0x80) == 0) /* 0x1ad3 */
    goto label_1ab5;
  return;
label_1ad6:
  RAM[0x28 + x] = tmp;
  do_1ad8(x, y, tmp);
}

/* MovePlayer */
static void do_1e63(void)
{
  int x, y, tmp;

  RAM[0x46] = 0;
  RAM[0x47] = 0;
  if (RAM[0x60] & 0x01) { /* 0x1e6c */
      RAM[0x46]++;
  }
  if (RAM[0x60] & 0x02) { /* 0x1e71 */
      RAM[0x46]--;
  }
  if (RAM[0x60] & 0x04) { /* 0x1e76 */
      RAM[0x47]--;
  }
  if (RAM[0x60] & 0x08) { /* 0x1e7b */
      RAM[0x47]++;
  }
  RAM[0x47] <<= 1;
  /* 0x1e81 */
  tmp = RAM[0x49];
  if (tmp == 2) {
      /* Jump */
      /* 0x1f81 */
label_1f81:
      RAM[0x46] = RAM[0x4b];
      RAM[0x89] = RAM[0x47];
      tmp = RAM[0x4a] >> 2;
      if (tmp >= 6) /* 0x1f8f */
	tmp = 6;
      tmp ^= 0xff;
      tmp += 3;
      RAM[0x47] = tmp;
      RAM[0x4a]++;
      if (RAM[0x41] == 0xdc) { /* 0x1fa0 */
	  RAM[0x47] = 0xff;
	  RAM[0x4a] = 0x0c;
	  goto label_2062;
      }
      /* 0x1fad */
      tmp = (int8_t)(RAM[0x44] + RAM[0x46]);
      if (tmp != 3) /* 0x1fb4 */
	  goto label_2016;
      if (RAM[0x89] == 0) /* 0x1fb8 */
	goto label_2016;
      if ((RAM[0x89] & 0x80) != 0) /* 0x1fba */
	goto label_1fed;
      /* 0x1fbc */
      x = RAM[0x42];
      y = RAM[0x43] + 1;
      tmp = Do_ReadMap(x, y); /* 0x1fc1 */
      if ((tmp & 2) != 0) /* 0x1fc6 */
	  goto label_1fdb;
      if (RAM[0x45] >= 4) /* 0x1fd1 */
	y++;
      tmp = Do_ReadMap(x, y); /* 0x1fd4 */
      if ((tmp & 2) == 0) /* 0x1fd7 */
	goto label_2016;
label_1fdb:
      RAM[0x49] = 1;
      tmp = RAM[0x45] + RAM[0x47];
      if (tmp & 1) /* 0x1fe6 */
	  RAM[0x47]++;
      goto label_20cd; /* 0x1fea */
label_1fed:
      x = RAM[0x42];
      y = RAM[0x43];
      tmp = Do_ReadMap(x, y); /* 0x1ff1 */
      if ((tmp & 2) == 0) /* 0x1ff6 */
	goto label_2016;
      x = RAM[0x42];
      y = RAM[0x43] + 1;
      tmp = Do_ReadMap(x, y); /* 0x1ffd */
      if ((tmp & 2) == 0) /* 0x2002 */
	goto label_2016;
      RAM[0x49] = 1;
      tmp = RAM[0x45] + RAM[0x47];
      if (tmp & 1) /* 0x1fe6 */
	  RAM[0x47]--;
      goto label_20cd;
label_2016:
      tmp = (uint8_t)(RAM[0x47] + RAM[0x45]);
      if (tmp == 0)
	goto label_2039;
      if ((tmp & 0x80) == 0) {
	  /* 0x204c */
	  if (tmp != 8)
	    goto label_2062;
	  x = RAM[0x42];
	  y = RAM[0x43];
	  tmp = Do_ReadMap(x, y); /* 0x2054 */
	  if ((tmp & 1) == 0) /* 0x2059 */
	    goto label_2062;
	  RAM[0x49] = 0;
	  goto label_2062;
      }
      /* 0x201f */
      x = RAM[0x42];
      y = RAM[0x43] - 1;
      tmp = Do_ReadMap(x, y); /* 0x2024 */
      if ((tmp & 1) == 0) /* 0x2029 */
	goto label_2062;
      RAM[0x49] = 0;
      RAM[0x47] = -RAM[0x45];
      goto label_2062; /* 0x2036 */
label_2039:
      x = RAM[0x42];
      y = RAM[0x43] - 1;
      tmp = Do_ReadMap(x, y); /* 0x203e */
      if ((tmp & 1) == 0) /* 0x2041 */
	goto label_2062;
      RAM[0x49] = 0;
      goto label_2062;
label_2062:
      if (RAM[0x55] == 0) /* 0x2064 */
	goto label_20bf;
      tmp = (uint8_t)(RAM[0x58] - 1);
      if (tmp >= RAM[0x40]) /* 0x206d */
	goto label_20bf;
      tmp = (uint8_t)(tmp + 0x0a);
      if (tmp < RAM[0x40]) /* 0x2073 */
	goto label_20bf;
      /* 0x2075 */
      RAM[0x8b] = RAM[0x41] - 0x11;
      RAM[0x8c] = RAM[0x41] - 0x13 + RAM[0x47];
      tmp = RAM[0x59];
      if (tmp == RAM[0x8b]) /* 0x2087 */
	goto label_208f;
      if (tmp >= RAM[0x8b]) /* 0x2089 */
	goto label_2099;
      if (tmp < RAM[0x8c]) /* 0x208d */
	goto label_2099;
label_208f:
      if (RAM[0x5b] != 0) /* 0x20a7 */
	tmp++;
      goto label_20ac; /* 0x2096 */
label_2099:
      tmp = RAM[0x5a];
      if (tmp == RAM[0x8b]) /* 0x209d */
	goto label_20a5;
      if (tmp >= RAM[0x8b])
	goto label_20bf;
      if (tmp < RAM[0x8c]) /* 0x20a3 */
	goto label_20bf;
label_20a5:
      if (RAM[0x5b] == 0) /* 0x20a7 */
	tmp++;
label_20ac:
      tmp -= RAM[0x8b];
      RAM[0x47] = tmp + 1;
      RAM[0x4a] = 0;
      RAM[0x49] = 4;
      goto label_20cd; /* 0x20bc */
label_20bf:
      if (do_2276()) {
	  RAM[0x46] = -RAM[0x46];
	  RAM[0x4b] = RAM[0x46];
      }
label_20cd:
      goto label_217b; /* 0x20cd */
  } else if (tmp == 3) {
      /* Fall */
      /* 0x20e3 */
      RAM[0x4a]++;
      tmp = RAM[0x4a];
      if (tmp < 4) { /* 0x20e9 */
	  RAM[0x46] = RAM[0x4b];
	  RAM[0x47] = 0xff;
	  goto label_2108; /* 0x20f3 */
      }
      /* 0x20f6 */
      RAM[0x46] = 0;
      tmp = RAM[0x4a] >> 2;
      if (tmp >= 4) /* 0x2100 */
	tmp = 3;
      RAM[0x47] = ~tmp;
label_2108:
      tmp = (int8_t)(RAM[0x47] + RAM[0x45]);
      if (tmp == 0) /* 0x210d */
	goto label_212b;
      if (tmp >= 0) /* 0x210f */
	goto label_213b;
      x = RAM[0x42];
      y = RAM[0x43] - 1;
      tmp = Do_ReadMap(x, y); /* 0x2116 */
      if ((tmp & 1) == 0) /* 0x211b */
	goto label_213b;
      RAM[0x49] = 0;
      RAM[0x47] = -RAM[0x45];
      goto label_213b; /* 0x2128 */
label_212b:
      x = RAM[0x42];
      y = RAM[0x43] - 1;
      tmp = Do_ReadMap(x, y); /* 0x2130 */
      if ((tmp & 1) != 0) /* 0x2135 */
	RAM[0x49] = 0;
label_213b:
      goto label_217b;
  } else if (tmp == 1) {
      /* Climb */
      /* 0x1f22 */
      if ((RAM[0x60] & 0x10) != 0) /* 0x1f26 */
	goto label_20d0;
      tmp = RAM[0x46];
      if (tmp == 0) /* 0x1f2d */
	goto label_1f4a;
      x = RAM[0x45];
      if (x != 0) /* 0x1f31 */
	goto label_1f4a;
      x = RAM[0x42];
      y = RAM[0x43] - 1;
      tmp = Do_ReadMap(x, y); /* 0x1f38 */
      if ((tmp & 1) == 0) /* 0x1f3d */
	goto label_1f4a;
      RAM[0x47] = 0;
      RAM[0x49] = 0;
      goto label_1f7a; /* 0x1f47 */
label_1f4a:
      RAM[0x46] = 0;
      if (RAM[0x47] == 0) /* 0x1f50 */
	goto label_1f7a;
      if (RAM[0x45] != 0) /* 0x1f54 */
	goto label_1f7a;
      if ((RAM[0x47] & 0x80) != 0) /* 0x1f56 */
	goto label_1f6c;
      x = RAM[0x42];
      y = RAM[0x43] + 2;
      tmp = Do_ReadMap(x, y); /* 0x1f60 */
      if ((tmp & 2) == 0) /* 0x1f65 */
	RAM[0x47] = 0;
      goto label_1f7a; /* 0x1f69 */
label_1f6c:
      x = RAM[0x42];
      y = RAM[0x43] - 1;
      tmp = Do_ReadMap(x, y); /* 0x1f71 */
      if ((tmp & 2) == 0) /* 0x1f76 */
	RAM[0x47] = 0;
label_1f7a:
      RAM[0x4c] = 0;
      goto label_217b;
  } else if (tmp != 0) {
      /* 0x213e */
      if ((RAM[0x60] & 0x10) != 0) /* 0x2142 */
	goto label_20d0;
      tmp = (uint8_t)(RAM[0x58] - 1);
      if (tmp >= RAM[0x40]) /* 0x214e */
	goto label_2156;
      tmp = (uint8_t)(tmp + 0x0a);
      if (tmp >= RAM[0x40]) /* 0x2154 */
	goto label_2160;
label_2156:
      RAM[0x4a] = 0;
      RAM[0x4b] = 0;
      RAM[0x49] = 3;
label_2160:
      RAM[0x47] = 1;
      tmp= RAM[0x46];
      if (tmp != 0) /* 0x2166 */
	RAM[0x4c] = tmp;
      if (do_2276()) { /* 0x216a */
	  RAM[0x46] = 0;
      }
      tmp = RAM[0x41];
      if (tmp < 0xdc) /* 0x2177 */
	goto label_217b;
      RAM[0x4f]++;
label_217b:
      do_2324(RAM[0x48]); /* 0x217d */
      RAM[0x40] += RAM[0x46];
      tmp = (uint8_t)(RAM[0x44] + RAM[0x46]);
      if ((tmp & 0x80) != 0) /* 0x218c */
	RAM[0x42]--;
      if (((tmp - 8) & 0x80) == 0) /* 0x2192 */
	RAM[0x42]++;
      RAM[0x44] = tmp & 7;
      RAM[0x41] += RAM[0x47];
      tmp = (uint8_t)(RAM[0x45] + RAM[0x47]);
      if ((tmp & 0x80) != 0) /* 0x21a6 */
	RAM[0x43]--;
      if (((tmp - 8) & 0x80) == 0) /* 0x21ac */
	RAM[0x43]++;
      RAM[0x45] = tmp & 7;
      x = 6;
      /* 0x21b6 */
      tmp = RAM[0x4c];
      if (tmp == 0) /* 0x21b8 */
	goto label_21c6;
      if ((tmp & 0x80) == 0) /* 0x21ba */
	goto label_21be;
      x = 9;
label_21be:
      RAM[0x88] = x;
      tmp = RAM[0x44] >> 1;
      goto label_21cd; /* 0x21c3 */
label_21c6:
      RAM[0x88] = 0x0c;
      tmp = RAM[0x45] >> 1;
label_21cd:
      x = 2;
      RAM[0x89] = x;
      if (RAM[0x89] != 0) { /* 0x21d3 */
	  tmp = (tmp & 1) << 1;
      }
      x = RAM[0x49];
      if (x != 1) /* 0x21dc */
	goto label_21e7;
      x = RAM[0x47];
      if (x != 0) /* 0x21e0 */
	goto label_21ed;
      tmp = 0;
      goto label_21ed; /* 0x21e4 */
label_21e7:
      x = RAM[0x46];
      if (x != 0) /* 0x21e9 */
	goto label_21ed;
      tmp = 0;
label_21ed:
      tmp += RAM[0x88];
      RAM[0x48] = tmp;
      do_2324(tmp); /* 0x21f2 */
      x = RAM[0x42];
      y = RAM[0x43];
      tmp = RAM[0x45];
      if (tmp >= 4) /* 0x21fd */
	y++;
      RAM[0x89] = y;
      tmp = Do_ReadMap(x, y); /* 0x2202 */
      RAM[0x88] = tmp;
      tmp &= 0x0c;
      if (tmp == 0) /* 0x2209 */
	goto label_2275;
      tmp &= 0x08;
      if (tmp != 0) /* 0x220d */
	goto label_2248;
      /* Got egg */
      RAM[0x39]--;
      /* 0x2211 */
      /* BEEP(6) */
      /* 0x221f */
      tmp = (RAM[0x88] >> 4) + RAM[0x4e];
      RAM[0x0510 + tmp]--;
      x = RAM[0x42];
      y = RAM[0x89];
      do_22fe(x, y); /* 0x2230 */
      tmp = (RAM[0x50] >> 2) + 1;
      if (tmp >= 0x0a) /* 0x223c */
	tmp = 0x0a;
      x = 5;
      do_1ab5(x, tmp);
      goto label_2275; /* 0x2245 */
label_2248:
      /* Got grain */
      /* BEEP(5) */
      /* 0x2256 */
      tmp = (RAM[0x88] >> 4) + RAM[0x4e];
      RAM[0x0520 + tmp]--;
      x = RAM[0x42];
      y = RAM[0x89];
      do_2311(x, y); /* 0x2267 */
      tmp = 5;
      x = 6;
      do_1ab5(x, tmp); /* 0x226e */
      RAM[0x1c] = 14;
label_2275:
      return; /* 0x2275 */
  } else {
      /* Walk */
      /* 0x1e9b */
      if (RAM[0x60] & 0x10) { /* 0x1e9f */
	  /* 0x20d0 */
label_20d0:
	  RAM[0x4a] = 0;
	  RAM[0x49] = 2;
	  tmp = RAM[0x46];
	  RAM[0x4b] = tmp;
	  if (tmp != 0) /* 0x20dc */
	    RAM[0x4c] = tmp;
	  goto label_1f81; /* 0x20e0 */
      } else if (RAM[0x47]) { /* 0x1ea6 */
	  /* 0x1ea8 */
	  if (RAM[0x44] == 3) { /* 0x1eac */
	      if ((RAM[0x47] & 0x80) == 0) { /* 0x1eb0 */
		  x = RAM[0x42];
		  y = RAM[0x43] + 2;
		  tmp = Do_ReadMap(x, y); /* 0x1ebe */
		  if ((tmp & 2) == 0) /* 0x1ebd */
		    goto label_1ed8;
		  goto label_1ecd;

	      }
	      /* 0x1ec1 */
	      x = RAM[0x42];
	      y = RAM[0x43] - 1;
	      tmp = Do_ReadMap(x, y); /* 0x1ec6 */
	      if ((tmp & 2) == 0) /* 0x1ecb */
		goto label_1ed8;
label_1ecd:
	      RAM[0x46] = 0;
	      RAM[0x49] = 1;
	      goto label_1f19; /* 0x1ed5 */
	  }
	  goto label_1ed8;
      } else {
label_1ed8:
	  /* 0x1ed8 */
	  RAM[0x47] = 0;
	  tmp = (int8_t)(RAM[0x44] + RAM[0x46]);
	  x = RAM[0x42];
	  if (tmp < 0)
	    x--;
	  else if (tmp >= 8)
	    x++;
	  y = RAM[0x43] - 1;
	  tmp = Do_ReadMap(x, y); /* 0x1ef0 */
	  tmp &= 1;
	  if (tmp == 0) { /* 0x1ef5 */
	      int n;
	      y = tmp;
	      x = 0xff;
	      n = (RAM[0x46] + RAM[0x44]) & 7;
	      if (n < 4) { /* 0x1f03 */
		  x = 1;
		  y++;
	      }
	      RAM[0x4b] = x;
	      RAM[0x4a] = y;
	      RAM[0x49] = 3;
	  }
	  /* 0x1f10 */
	  if (do_2276()) { /* 0x1f13 */
	      RAM[0x46] = 0;
	  }
label_1f19:
	  if (RAM[0x46]) { /* 0x1f1b */
	      RAM[0x4c] = RAM[0x46];
	  }
	  goto label_217b;
      }
  }
}

/* MoveSound */
static void do_0c38(void)
{
  int tmp;
  if (!(RAM[0x46] || RAM[0x47]))
    return;
  if ((RAM[0x38] & 1) != 0)
    return;
  tmp = RAM[0x49];
  if (tmp == 0) { /* 0x0c48 */
      tmp = 64;
      goto label_0c8b;
  }
  if (tmp == 1) { /* 0x0c51 */
      tmp = 96;
      goto label_0c8b;
  }
  if (tmp != 2) /*0x0c5a */
    goto label_0c76;
  tmp = RAM[0x4a];
  if (tmp >= 0x0b) { /* 0x0c60 */
      tmp = (uint8_t)(0xbe - (RAM[0x4a] * 2));
      goto label_0c8b;
  }
  tmp = (uint8_t)(0x96 - (RAM[0x4a] * 2));
  goto label_0c8b;
label_0c76:
  if (tmp == 3) { /* 0x0c78 */
    tmp = (uint8_t)(0x6e - (RAM[0x4a] * 2));
    goto label_0c8b;
  }
  if (RAM[0x46] == 0) /* 0x0c86 */
    return;
  tmp = 0x64;
label_0c8b:
  /* BEEP(tmp) */
  while(0);
}

/* MoveLift */
static void do_2374(void)
{
  int y;

  if (RAM[0x55] == 0)
    return;
  if (RAM[0x5b] == 0)
    y = RAM[0x59];
  else
    y = RAM[0x5a];
  Do_RenderSprite(RAM[0x58], y, 5, 1); /* 0x2392 */
  y += 2;
  if (y == 0xe0)
    y = 6;
  Do_RenderSprite(RAM[0x58], y, 5, 1); /* 0x23af */
  if (RAM[0x5b] == 0) {
      RAM[0x59] = y;
  } else {
      RAM[0x5a] = y;
  }
  RAM[0x5b] ^= 0xff;
}

/* popcount */
static int do_25a9(void)
{
  int x;
  int tmp;

  x = 0;
  tmp = RAM[0x8d];
label_25ad:
  if ((tmp & 1) != 0) /* 0x25ae */
    x++;
  tmp >>= 1;
  if (tmp != 0) /* 0x25b3 */
    goto label_25ad;
  return x;
}

static void do_1aa4(void)
{
  int tmp;
  int c1;
  int c2;

  tmp = (RAM[0x66] & 0x48) + 0x38;
  c1 = (tmp & 0x40) != 0;
  c2 = RAM[0x69] >> 7;
  RAM[0x69] = (RAM[0x69] << 1) | c1;
  c1 = RAM[0x68] >> 7;
  RAM[0x68] = (RAM[0x68] << 1) | c2;
  c2 = RAM[0x67] >> 7;
  RAM[0x67] = (RAM[0x67] << 1) | c1;
  RAM[0x66] = (RAM[0x66] << 1) | c2;
}

static void do_2702(void)
{
  int tmp;
  int x;
  int y;
  tmp = RAM[0x88];
  y = tmp;
  x = (uint8_t)((tmp << 2) + RAM[0x88] + 0x91);
  tmp = RAM[0x1d + y];
  y = 0xe7;
  Do_RenderDigit(x, y, tmp);
}

static void do_2715(void)
{
  int tmp;
  int x;
  int y;
  tmp = RAM[0x88];
  y = tmp;
  x = (uint8_t)((tmp << 2) + RAM[0x88] + 0x66);
  tmp = RAM[0x3a + y];
  y = 0xe7;
  Do_RenderDigit(x, y, tmp);
}

/* MoveDucks */
static void do_2407(void)
{
  int tmp;
  int y;
  int x;
  int flag;

  /* Big Duck.  */
  RAM[0x38]++;
  tmp = RAM[0x38];
  if (tmp == 8) {
      RAM[0x38] = 0;
      goto label_2420;
  }
  if (tmp == 4) /* 0x2418 */
    goto label_269d;
  goto label_24b5;
label_2420:
  RAM[0x8b] = RAM[0x34] & 2;
  if (RAM[0x35] == 0) /* 0x2428 */
    goto label_2494;
  tmp = (uint8_t)(RAM[0x30] + 4);
  if (tmp >= RAM[0x40]) /* 0x2431 */
    goto label_2444;
  RAM[0x32]++;
  if (((RAM[0x32] - 6) & 0x80) == 0) /* 0x2439 */
    RAM[0x32]--;
  RAM[0x8b] = 0;
  goto label_2452; /* 0x2441 */
label_2444:
  RAM[0x32]--;
  if (((RAM[0x32] - 0xfb) & 0x80) != 0)  /* 0x244a */
    RAM[0x32]++;
  RAM[0x8b] = 2;
label_2452:
  tmp = RAM[0x41] + 4;
  if (tmp < RAM[0x31]) /* 0x2459 */
    goto label_2468;
  RAM[0x33]++;
  if (((RAM[0x33] - 6) & 0x80) == 0)  /* 0x2461 */
    RAM[0x33]--;
  goto label_2472;
label_2468:
  RAM[0x33]--;
  if (((RAM[0x33] - 0xfb) & 0x80) != 0)  /* 0x246e */
    RAM[0x33]++;
label_2472:
  tmp = (uint8_t)(RAM[0x31] + RAM[0x33]);
  if (tmp >= 0x28) /* 0x2479 */
    goto label_2483;
  RAM[0x33] = ~RAM[0x33] + 1;
label_2483:
  tmp = (uint8_t)(RAM[0x30] + RAM[0x32]);
  if (tmp < 0x90) /* 0x248a */
    goto label_2494;
  RAM[0x32] = ~RAM[0x32] + 1;
label_2494:
  do_2336(RAM[0x34]); /* 0x2496 */
  RAM[0x30] += RAM[0x32];
  RAM[0x31] += RAM[0x33];
  RAM[0x34] = ((RAM[0x34] & 1) ^ 1) | RAM[0x8b];
  do_2336(RAM[0x34]); /* 0x24b1 */
  return;
label_24b5:
  RAM[0x36]--;
  x = RAM[0x36];
  if ((x & 0x80) != 0) { /* 0x24b9 */
      x = RAM[0x37];
      RAM[0x36] = x;
  }
  if (x >= RAM[0x57]) /* 0x24c1 */
    return;
  /* Move little duck.  */
  RAM[0x88] = x;
  tmp = RAM[0x0414 + x];
  if (tmp == 1) /* 0x24cb */
    goto label_25ef;
  if (tmp >= 1) /* 0x24d0 */
    goto label_25b6;
  RAM[0x8b] = RAM[0x040a + x];
  RAM[0x8c] = RAM[0x040f + x];
  RAM[0x8d] = 0;
  x = RAM[0x8b] - 1;
  y = RAM[0x8c] - 1;
  tmp = Do_ReadMap(x, y); /* 0x24e9 */
  if ((tmp & 1) != 0)
    RAM[0x8d] = 1;
  x = RAM[0x8b] + 1;
  y = RAM[0x8c] - 1;
  tmp = Do_ReadMap(x, y); /* 0x24f8 */
  if ((tmp & 1) != 0)
    RAM[0x8d] |= 2;
  x = RAM[0x8b];
  y = RAM[0x8c] - 1;
  tmp = Do_ReadMap(x, y); /* 0x250a */
  if ((tmp & 2) != 0)
    RAM[0x8d] |= 8;
  x = RAM[0x8b];
  y = RAM[0x8c] + 2;
  tmp = Do_ReadMap(x, y); /* 0x251d */
  if ((tmp & 2) != 0)
    RAM[0x8d] |= 4;
  x = do_25a9(); /* 0x252a */
  if (x == 1) { /* 0x252f */
      x = RAM[0x88];
      RAM[0x041e + x] = RAM[0x8d];
      goto label_257b; /* 0x2535 */
  }
  x = RAM[0x88];
  tmp = RAM[0x041e + x];
  if (tmp < 4) { /* 0x2542 */
      tmp ^= 0xfc;
  } else {
      tmp ^= 0xf3;
  }
  RAM[0x8d] &= tmp;
  x = do_25a9(); /* 0x254f */
  if (x == 1) { /* 0x2554 */
      x = RAM[0x88];
      RAM[0x041e + x] = RAM[0x8d];
      goto label_257b; /* 0x255d */
  }
  RAM[0x8e] = RAM[0x8d];
label_2564:
  do_1aa4(); /* 0x2564 */
  RAM[0x8d] = RAM[0x66] & RAM[0x8e];
  x = do_25a9(); /* 0x256d */
  if (x != 1) /* 0x2572 */
    goto label_2564;
  x = RAM[0x88];
  RAM[0x041e + x] = RAM[0x8d];
label_257b:
  x = RAM[0x88];
  tmp = RAM[0x041e + x];
  tmp &= 3;
  if (tmp == 0) /* 0x2582 */
    goto label_25ef;
  tmp &= 1;
  if (tmp == 0) /* 0x2586 */
    goto label_2593;
  x = RAM[0x8b] - 1;
  y = RAM[0x8c];
  tmp = Do_ReadMap(x, y); /* 0x258d */
  goto label_259b;
label_2593:
  x = RAM[0x8b] + 1;
  y = RAM[0x8c];
  tmp = Do_ReadMap(x, y); /* 0x2598 */
label_259b:
  tmp &= 8;
  if (tmp == 0)
    goto label_25ef;
  x = RAM[0x88];
  tmp = 2;
  RAM[0x0414 + x] = tmp;
  goto label_25ef; /* 0x25a6 */
label_25b6:
  if (tmp != 4)
    goto label_25ef;
  tmp = RAM[0x041e + x];
  RAM[0x8b] = RAM[0x040a + x];
  y = RAM[0x040f + x];
  RAM[0x8c] = y;
  x = RAM[0x8b] - 1;
  tmp &= 1;
  if (tmp == 0) /* 0x25cc */
    x += 2;
  RAM[0x8d] = x;
  tmp = Do_ReadMap(x, y); /* 0x25d2 */
  RAM[0x89] = tmp;
  tmp &= 8;
  if ((tmp & 8) == 0) /* 0x25d9 */
    goto label_25ef;
  tmp = RAM[0x89];
  tmp = (tmp >> 4) + RAM[0x4e];
  x = tmp;
  RAM[0x0520 + x]--;
  x = RAM[0x8d];
  y = RAM[0x8c];
  do_2311(x, y); /* 0x25ec */
label_25ef:
  do_234b(RAM[0x88]);
  x = RAM[0x88];
  tmp = RAM[0x0414 + x];
  if (tmp >= 2) /* 0x25f9 */
    goto label_2675;
  tmp = RAM[0x041e + x];
  if ((tmp & 1) != 0) /* 0x25ff */
    goto label_2633;
  if ((tmp & 2) != 0) /* 0x2602 */
    goto label_2649;
  if ((tmp & 4) != 0) /* 0x2605 */
    goto label_261d;
  RAM[0x0405 + x] -= 4;
  tmp = RAM[0x0414 + x];
  if (tmp != 0) /* 0x2613 */
    RAM[0x040f + x]--;
  tmp = 4;
  goto label_265f;
label_261d:
  RAM[0x0405 + x] += 4;
  tmp = RAM[0x0414 + x];
  if (tmp != 0) /* 0x2629 */
    RAM[0x040f + x]++;
  tmp = 4;
  goto label_265f;
label_2633:
  RAM[0x0400 + x] -= 4;
  tmp = RAM[0x0414 + x];
  if (tmp != 0) /* 0x263f */
    RAM[0x040a + x]--;
  tmp = 2;
  goto label_265f;
label_2649:
  RAM[0x0400 + x] += 4;
  tmp = RAM[0x0414 + x];
  if (tmp != 0) /* 0x263f */
    RAM[0x040a + x]++;
  tmp = 0;
  goto label_265f;
label_265f:
  y = RAM[0x0414 + x] ^ 1;
  RAM[0x0414 + x] = y;
  tmp += y;
  RAM[0x0419 + x] = tmp;
  do_234b(x);
  return;
label_2675:
  tmp = RAM[0x0414 + x] << 1;
  tmp &= 0x1f;
  RAM[0x0414 + x] = tmp;
  if (tmp != 0)
    tmp = 6;
  y = RAM[0x041e + x];
  if (y == 1) /* 0x2687 */
    tmp += 2;
  y = RAM[0x0414 + x];
  if (y == 8) /* 0x2691 */
    tmp++;
  RAM[0x0419 + x] = tmp;
  do_234b(x);
  return;
label_269d:
  /* Update bonus/timer.  */
  tmp = RAM[0x1c];
  if (tmp != 0) { /* 0x26a3 */
      RAM[0x1c]--;
      return; /* 0x26a7 */
  }
  RAM[0x88] = 2;
label_26ac:
  do_2702();
  x = RAM[0x88];
  RAM[0x1d + x]--;
  flag = (RAM[0x1d + x] & 0x80) != 0;
  if (flag) /* 0x26b4 */
    RAM[0x1d + x] = 9;
  do_2702();
  RAM[0x88]--;
  if (flag) /* 0x26c0 */
    goto label_26ac;
  tmp = (uint8_t)(RAM[0x1d] + RAM[0x1e] + RAM[0x1f]);
  if (tmp == 0) { /* 0x26c9 */
      RAM[0x4f]++;
      return; /* 0x26cd */
  }
  tmp = RAM[0x1f];
  if (tmp != 0 && tmp != 5)
    return; /* 0x26d6 */
  tmp = RAM[0x3d];
  if (tmp != 0) /* 0x26d9 */
    return;
  RAM[0x88] = 2;
label_26e0:
  do_2715();
  x = RAM[0x88];
  RAM[0x3a + x]--;
  flag = ((RAM[0x3a + x] & 0x80) != 0);
  if (flag) /* 26e8 */
      RAM[0x3a + x] = 9;
  do_2715();
  RAM[0x88]--;
  if (flag) /* 0x26f4 */
    goto label_26e0;
  tmp = (uint8_t)(RAM[0x3a] + RAM[0x3b] + RAM[0x3c]);
  if (tmp == 0) /* 0x26fd */
    RAM[0x3d]++;
}

/* MaybeAddExtraLife */
static void do_2f49(void)
{
  if (RAM[0x3e] == 0)
    return;
  RAM[0x3e] = 0;
  do_2f5a();
  RAM[0x20 + RAM[0x5d]]++;
}

/* CollisionDetect */
static void do_2728(void)
{
  int x;
  int tmp;
  /* Little ducks */
  if (RAM[0x57] == 0)
    goto label_2758;
  x = 0;
label_2730:
  tmp = (uint8_t)((RAM[0x0400 + x] - RAM[0x40]) + 5);
//printf ("x%d %d\n", x, tmp);
  if (tmp >= 0x0b) /* 0x273d */
    goto label_2750;
  tmp = (uint8_t)((RAM[0x0405 + x] - 1) - RAM[0x41] + 0xe);
  if (tmp >= 0x1d) /* 0x274c */
    goto label_2750;
  RAM[0x4f]++;
label_2750:
  x++;
  if (x < RAM[0x57])
    goto label_2730;
label_2758:
  /* Big duck */
  if (RAM[0x35] == 0)
    return;
  tmp = (uint8_t)(RAM[0x30] + 4 - RAM[0x40] + 5);
  if (tmp >= 0x0b) /* 0x2769 */
    return;
  tmp = (uint8_t)(RAM[0x31] - 5 - RAM[0x41] + 0x0e);
  if (tmp >= 0x1d) /* 0x2777 */
    return;
  RAM[0x4f]++;
}

/* TimerTickDown */
static void do_26dc(void)
{
  int x;
  int flag;
  RAM[0x88] = 2;
label_26e0:
  do_2715();
  x = RAM[0x88];
  RAM[0x3a + x]--;
  flag = ((RAM[0x3a + x] & 0x80) != 0);
  if (flag) { /* 0x26e8 */
      RAM[0x3a + x] = 9;
  }
  do_2715();
  RAM[0x88]--;
  if (flag)
    goto label_26e0;
  if (RAM[0x3a] + RAM[0x3b] + RAM[0x3c] == 0)
    RAM[0x3d]++;
}

/* ResetLevel? */
static void do_2e6b(void)
{
  int x;
  int i;
  x = RAM[0x5d];
  RAM[0x24 + x] = RAM[0x50];
  x = RAM[0x4e];
  for (i = 0; i < 8; i++) /* 0x2e7f */
    RAM[0x28 + i] = RAM[0x0500 + x + i];
  for (i = 0; i < 4; i++) /* 0x2e8f */
    RAM[0x3a + i] = RAM[0x0508 + x + i];
}

static void do_2dfe(void)
{
  int a, b;
  int i;
  a = RAM[0x50] + 1;
  b = RAM[0x5d] << 6;
  if (a >= 10)
    a = 9;
  RAM[0x0508 + b] = a;
  RAM[0x0509 + b] = 0;
  RAM[0x050a + b] = 0;
  RAM[0x050b + b] = 0;
  for (i = 0x10; i > 0; i--) {
      RAM[0x0510 + b + i] = 0;
      RAM[0x0520 + b + i] = 0;
  }
}

static void do_2e2d(void)
{
  int player;
  int level;
  int o, y;
  player = RAM[0x5d];
  level = RAM[0x24 + player];
  RAM[0x50] = level;
  o = (player << 6) & 0xff;
  RAM[0x4e] = o;
  for (y = 0; y < 8; y++) {
      RAM[0x28 + y] = RAM[0x0500 + o + y];
  }
  for (y = 0; y < 4; y++) {
      RAM[0x3a + y] = RAM[0x0508 + o + y];
  }
  RAM[0x3f] = (player * 0x22) + 0xd;
}

static void do_2f7c(int addr)
{
  /* Play tune */
}

/* EndOfFrame */
static int do_29f0(void)
{
  int tmp;
  if (RAM[0x4f] != 0)
    goto label_2a39;
  if (RAM[0x41] < 0x11)
    goto label_2a39;
  if (RAM[0x39] == 0) /* 0x29fc */
    goto label_2a05;
  if ((RAM[0x60] & 0x80) != 0) /* 0x2a00 */
    goto label_29ae;
  /* Next frame */
  return 0x29d8;
label_2a05:
  /* Level complete */
  if (RAM[0x3d] != 0)
    goto label_2a2b;
label_2a09:
  do_1ab5(6, 1);
  do_26dc();
  do_2f49();
  tmp = RAM[0x3c];
  if (tmp == 0 || tmp == 5) { /* 0x2a1c */
      /* BEEP(?) */
  }
  /* 0x2a27 */
  if (RAM[0x3d] == 0)
    goto label_2a09;
label_2a2b:
  /* Advance to next level */
  RAM[0x50]++;
  do_2e6b(); /* 0x2a2d */
  do_2dfe();
  do_2e2d();
  return 0x29cf;
label_2a39:
  /* Died */
  do_2e6b();
  do_2f7c(0x2fa6); /* 0x2a40 */
  tmp = --RAM[0x20 + RAM[0x5d]];
  if (tmp != 0)
    goto label_2a70;
  /* Clear Screen */
  /* "Game Over" */
  /* 0x2a64 */
  /* Highscores.  */
  if (--RAM[0x5f] == 0)
    goto label_2a87;
label_2a70:
  /* Select next player. */
  tmp = RAM[0x5d];
  do {
      tmp = (tmp + 1) & 3;
  } while (tmp >= RAM[0x5e] || RAM[0x20 + tmp] == 0);
  RAM[0x5d] = tmp;
  do_2e2d(); /* 0x2a81 */
  return 0x29b4;
label_2a87:
  /* No players left. */
  /* Goto menu */
  goto label_29ae;
label_29ae:
  return 0x29ae;
}


void start_game()
{
  int i, j;
  RAM[0x5e] = RAM[0x5f] = NUM_PLAYERS;
  /* Lives */
  RAM[0x20] = RAM[0x21] = RAM[0x22] = RAM[0x23] = 5;
  /* Level */
  RAM[0x24] = RAM[0x25] = RAM[0x26] = RAM[0x27] = 0;
  for (i = 0; i < 0x100; i += 0x40) {
      for (j = 0; j < 8; j++) {
	RAM[0x0500 + i + j] = 0;
      }
  }
  for (i = 3; i >= 0; i--) {
      RAM[0x5d] = i;
      do_2dfe();
  }
  do_2e2d();
}

/* 0x2dc0 */
static void SetupLevel(void)
{
  int arg;

  arg = RAM[0x50];
  RAM[0x5c] = arg & 7;
  RAM[0x4d] = arg >> 3;
  RAM[0x35] = (arg > 7);
  RAM[0x38] = 0;
  RAM[0x36] = 0;
  RAM[0x37] = (arg < 32) ? 8 : 5;
  RAM[0x3e] = 0;
  RAM[0x4f] = 0;
  RAM[0x1c] = 0;
  RAM[0x66] = 0x76;
  RAM[0x67] = 0x76;
  RAM[0x68] = 0x76;
  RAM[0x69] = 0x76;
}

SDLKey keys[8] = {
    SDLK_PERIOD,
    SDLK_COMMA,
    SDLK_z,
    SDLK_a,
    SDLK_SPACE
};
/* Read inputs, and wait until the start of the next frame.  */
static void PollKeys(void)
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
		      RAM[0x60] |= 1 << i;
		  else
		      RAM[0x60] &= ~(1 << i);
	      }
	  }
	  break;
      }
  }
}

static void ClearScreen(void)
{
  memset(pixels, 0, 160*256);
}

static void RenderFrame(void)
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

void run_game()
{
new_game:
  start_game();
next_player:
  /* 0x29b4 -29cc "Get Ready" */
next_level:
  SetupLevel();
  ClearScreen();
  do_1b38();
  do_2e92();
next_frame:
  PollKeys();
  do_1e63();
  do_0c38();
  do_2374();
  do_2407();
  do_2f49();
  do_2728();
  RenderFrame();
  switch (do_29f0()) {
  case 0x29d8:
      goto next_frame;
  case 0x29cf:
      goto next_level;
  case 0x29b4:
      goto next_player;
  case 0x29ae: /* Menu */
      goto new_game;
  default:
      abort();
  }
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

  SDL_AddTimer(30, do_timer, NULL);

  run_game();
}
