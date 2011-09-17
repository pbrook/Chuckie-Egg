/* ncurses setup and rendering code.
   Copyright (C) 2011 Paul Brook
   This code is licenced under the GNU GPL v3 */
#include "chuckie.h"
#include "raster.h"
#include "ncurses.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

raster_hooks *raster = NULL;

/* Read inputs, and wait until the start of the next frame.  */
void PollKeys(void)
{
    int c;
    static int last_mode;

    if (player_mode != last_mode) {
	if (player_mode == 0) {
	    button_ack |= 0xc;
	} else if (player_mode == 1) {
	    button_ack |= 3;
	} else if (player_mode == 4) {
	    button_ack |= 0xf;
	}
	last_mode = player_mode;
    }

    buttons &= ~button_ack;
    button_ack = 0;
    c = getch();
    switch (c) {
    case '.':
	buttons |= 1;
	buttons &= ~2;
	break;
    case ',':
	buttons |= 2;
	buttons &= ~1;
	break;
    case 'z':
	buttons |= 4;
	buttons &= ~8;
	break;
    case 'a':
	buttons |= 8;
	buttons &= ~4;
	break;
    case ' ':
	buttons |= 0x10;
	break;
    case 'q':
    case 'Q':
	endwin();
	exit(0);
	break;
    case 'l':
	cheat = 1;
	break;
    }
    usleep(30000);
}

#define ATTR_EGG COLOR_PAIR(1) | A_BOLD
#define ATTR_DUCK COLOR_PAIR(4) | A_BOLD
#define ATTR_PLAYER COLOR_PAIR(1) | A_BOLD
#define ATTR_LIFT COLOR_PAIR(1) | A_BOLD
#define ATTR_BIG_DUCK COLOR_PAIR(1) | A_BOLD

static const char *duck_left[2]   = {"<\\", "DD"};
static const char *duck_right[2]  = {"/>", "DD"};
static const char *duck_up[2]     = {"/\\", "DD"};
static const char *duck_down[2]   = {"\\/", "DD"};
static const char *player_top = "AA";
static const char *player_bottom = "##";
static const char *big_duck[2][2][3] = {
    {
	{"_/===>", "|---| ", "\\---/ "},
	{"_/===>", "|~~~| ", "\\---/ "}
    }, {
	{"<===\\_", " |---|", " \\---/"},
	{"<===\\_", " |~~~|", " \\---/"}
    }
};



void RenderFrame(void)
{
    int x;
    int y;
    const char *c;
    int type;
    int n;
    int dir;
    const char **p;

    for (y = 0; y < 25; y++) {
	move(24 - y, 0);
	for (x = 0; x < 20; x++) {
	    type = levelmap[y * 20 + x];
	    if (type & TILE_LADDER) {
		attrset(COLOR_PAIR(2));
		c = "II";
	    } else if (type & TILE_WALL) {
		c = "==";
	       attrset(COLOR_PAIR(3));
	    } else if (type & TILE_EGG) {
		c = "{}";
		attrset(ATTR_EGG);
	    } else if (type & TILE_GRAIN) {
		c = "..";
	       	attrset(COLOR_PAIR(2));
	    } else {
		c = "  ";
	    }
	    addstr(c);
	}
    }

    attrset(ATTR_DUCK);
    for (n = 0; n < num_ducks; n++) {
	x = duck[n].x >> 2;
	y = 25 - (duck[n].y >> 3);
	dir = duck[n].dir;
	if (dir == DIR_R) {
	    p = duck_right;
	} else if (dir == DIR_L) {
	    p = duck_left;
	} else if (dir == DIR_UP) {
	    p = duck_up;
	} else if (dir == DIR_DOWN) {
	    p = duck_down;
	}
	mvaddstr(y, x, p[0]);
	mvaddstr(y + 1, x, p[1]);
    }

    attrset(ATTR_PLAYER);
    x = player_x >> 2;
    y = 25 - (player_y >> 3);
    mvaddstr(y, x, player_top);
    if (y < 24)
	mvaddstr(y + 1, x, player_bottom);

    if (have_lift) {
	attrset(ATTR_LIFT);
	x = lift_x >> 2;
	for (n = 0; n < 2; n++) {
	    y = 25 - (lift_y[n] >> 3);
	    mvaddstr(y, x, "----");
	}
    }

    x = big_duck_x >> 2;
    y = 25 - (big_duck_y >> 3);
    for (n = 0; n < 3; n++) {
	mvaddstr(y + n, x, big_duck[big_duck_dir][big_duck_frame][n]);
    }

    refresh();
}

int main(int argc, const char *argv[])
{
    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);

    run_game();

    endwin();

    return 0;
}

