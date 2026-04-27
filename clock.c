#include "clock.h"

//Reference Manual Rev 6 Used for page numbers
//This version manual is in Keil Uvision under books
//sets the system clock to 100MHZ
void initSystemClockTo100Mhz(void)
{
	//pg 128
	RCC->CR |= RCC_CR_HSION; //enable internal high speed clock
	while(!(RCC->CR & RCC_CR_HSIRDY)); //waiting for clock to be enabled
	
	//setting flash latency to 3 clock cycle for 100MHZ
	FLASH->ACR &= ~FLASH_ACR_LATENCY;
	FLASH->ACR |= FLASH_ACR_LATENCY_3WS;
	// Enable Prefetch, Instruction Cache, and Data Cache for performance
	FLASH->ACR |= FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN; //more details pg. 67
	
	//---set up PLL for 100MHZ pg.129---
	//clearing bit
	RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLP_Msk | RCC_PLLCFGR_PLLM_Msk |
										RCC_PLLCFGR_PLLN_Msk);
	//PLLP = 2 PLLN = 100 PLLM = 8
	RCC->PLLCFGR |= (0x64 << 6) | 0x8;
	RCC->PLLCFGR &= ~(1<<22);
	RCC->CR |= RCC_CR_PLLON; //enbaling PLL
	while(!(RCC_CR_PLLRDY & RCC->CR)); //wating for PLL to be ready
	
	//APB1 Max Clock is 45MHZ and APB2 Max Clock is 90MHZ
	// AHB = /1 (100 MHz), APB1 = /4 (25 MHz), APB2 = /2 (50 MHz) 
	// For Timers do the clock speed * 2 (e.g. TIM2 is running at 25MHX * 2 = 50MHZ)
	RCC->CFGR &= ~(RCC_CFGR_HPRE_Msk | RCC_CFGR_PPRE1_Msk | RCC_CFGR_PPRE2_Msk);
	RCC->CFGR |= (RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2);

	//selecting PLL clock as system clock
	RCC->CFGR &= ~(RCC_CFGR_SW_Msk); //clearing clock
	RCC->CFGR |= RCC_CFGR_SW_1; //setting PLL as system clock
	while((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL);
}

void delayMs(uint32_t ms)
{
	SysTick->LOAD = 100000 - 1; // (0.001 * 100MHz) - 1
	SysTick->VAL = 0;           // clear current value
	SysTick->CTRL = 0x5;        // enable timer and select system clock
	
	for(;ms > 0; ms--)
		while((SysTick->CTRL & 0x10000) == 0); // Wait for COUNTFLAG
	
	SysTick->CTRL = 0;          // disable timer
}

void delayUs(uint32_t us)
{
	SysTick->LOAD = 100 - 1; // (1E-6 * 100MHz) - 1
	SysTick->VAL = 0;           // clear current value
	SysTick->CTRL = 0x5;        // enable timer and select system clock
	
	for(;us > 0; us--)
		while((SysTick->CTRL & 0x10000) == 0); // Wait for COUNTFLAG
	
	SysTick->CTRL = 0;          // disable timer
}