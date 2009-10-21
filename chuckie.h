#ifndef CHUCKIE_E
#define CHUCKIE_H

#include <stdint.h>

extern uint8_t pixels[160 * 256];
extern uint8_t buttons;

void PollKeys(void);
void RenderFrame(void);

void run_game(void);

void sound_start(int);

#endif
