#ifndef CHUCKIE_E
#define CHUCKIE_H

#include <stdint.h>

extern uint8_t pixels[160 * 256];
extern uint8_t buttons;

void PollKeys(void);
void RenderFrame(void);

void run_game(void);

#define BUFSIZE 512
#define FREQ 44100
extern short next_buffer[BUFSIZE];
void mix_buffer(void);
void sound_start(int, int);
void init_sound(void);

#endif
