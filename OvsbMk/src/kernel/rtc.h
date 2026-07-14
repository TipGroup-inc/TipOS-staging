#include <stdint.h>
#ifndef RTC_H
#define RTC_H
typedef struct { uint8_t s, m, h, dy, mo; uint16_t yr; } rtc_time;
void rtc_read(rtc_time *t);
#endif
