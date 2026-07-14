#ifndef PIT_H
#define PIT_H
#include <stdint.h>
extern volatile uint64_t timer_ticks;
void timer_tick_handler(void);
void pit_init(void);
void sleep_ms(uint64_t ms);
#endif
