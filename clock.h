#ifndef CLOCK_H_
#define CLOCK_H_

#include <stdint.h>
#include "stm32f446xx.h"

void initSystemClockTo100Mhz(void);

//delayMs and delayUS utalizes stick 
void delayMs(uint32_t ms);
void delayUs(uint32_t us);

#endif