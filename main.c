#include "stm32f446xx.h"
#include <stdint.h>
#include "clock.h"
#include <stdbool.h>
#include "tft.h"

#define MEMORY 100
#define FRAME 10

#define DATA_PIN  (1 << 7)   // PC7
#define CLOCK_PIN (1 << 9)   // PA9
#define CLEAR_PIN (1 << 8)   // PA8

volatile uint8_t HorizontalSensor[FRAME];
volatile uint8_t VerticalSensor[FRAME];

volatile uint8_t X_AXIS[MEMORY];
volatile uint8_t Y_AXIS[MEMORY];

volatile uint8_t CurrentIRLED;
volatile uint32_t GridMemory; 

//Flags
volatile bool new_data_available = false;
volatile uint8_t stateV= 0x1;
volatile uint8_t stateH= 0x1;

//12-bit = (2.0/3.3)*4095   
#define ADC_THRESHOLD 3723 //3.0
 
// ADC channels for the two sensors
#define VERT_CHANNEL   10  // PC0 -> ADC1_IN10 (VerticalSensor)
#define HORIZ_CHANNEL  11  // PC1 -> ADC1_IN11 (HorizontalSensor)


/*
* setting PC7 DATA
* setting PA9 Clock
* setting PA8 CLEAR
* setting PC0 as IR-Sensor Input VerticalSensor
* setting PC1 as IR-Sensor Input HorizontalSensor
*/

//pin a1 connected to rst
//pin a0 connected to tdc
// a5 sck (vv SPI1 vv)
// a6 miso
// a7 mosi
// b6 cs
void initGPIO(void)
{
	RCC->AHB1ENR |= 0x7; 	//enabling GPIOA, B and C
	
	//GPIO for IR LED
	GPIOC->MODER = (GPIOC->MODER & ~(0x3 << 14)) | (1<<14); //PC7 DATA
	GPIOA->MODER = (GPIOA->MODER & ~(0x3 << 18)) | (1<<18);	//PA9 CLOCK
	GPIOA->MODER = (GPIOA->MODER & ~(0x3 << 16)) | (1<<16); //PA8 CLEAR
	
	
	GPIOC->BSRR = DATA_PIN << 16;
	GPIOA->BSRR = CLEAR_PIN;
	GPIOA->BSRR = CLOCK_PIN << 16;
}


/*
* ADC Init 
*PC0 (IN10) and PC1 (IN11)
* Both sensors share the same ADC; we just change the channel in SQR3
* before each conversion.
*/
void initADC(void)
{
	// Enabling GPIO C Clock
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
 
	//Enblaing APB2 for GPIO C
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
 
	// PC0 and PC1 to analog mode 
	GPIOC->MODER |=  (0x3 << (0));  
	GPIOC->MODER |=  (0x3 << (2)); 
 
	// No pull-up or pull-down
	GPIOC->PUPDR &= ~((0x3 << (0 * 2)) | (0x3 << (1 * 2)));
 
	//default PCLK2/2 prescaler
	ADC->CCR &= ~ADC_CCR_ADCPRE;
 
	// ADC1 config
	ADC1->CR1 = 0; // 12-bit resolution (default), no scan, no interrupts
	ADC1->CR2 = 0; // single conversion, right-aligned, SW trigger
	ADC1->SQR1 = 0; // 1 conversion in the regular sequence (L = 0)
 
	// Sample time 56 cycles 
	// SMPR1 holds SMP10..SMP18; bit offset = (channel - 10) * 3.
	ADC1->SMPR1 &= ~((0x7 << ((10 - 10) * 3)) | (0x7 << ((11 - 10) * 3)));
	ADC1->SMPR1 |=  ((0x3 << ((10 - 10) * 3)) | (0x3 << ((11 - 10) * 3)));
 
	ADC1->CR2 |= ADC_CR2_ADON; //Enable ADC
	delayUs(10);             
}
 
/*
*Get the ADC value for the given channel 
* 12-bit result fro 0 to 4095
*/
uint16_t readADC(uint8_t channel)
{
	ADC1->SQR3 = channel & 0x1F;  // set channel as the 1st (only) conversion
	ADC1->CR2 |= ADC_CR2_SWSTART; // start conversion
	while (!(ADC1->SR & ADC_SR_EOC)); // wait for end-of-conversion
	return (uint16_t)ADC1->DR; 
}
 
/*
* 1 if blocked 0 if not blocked
*/
static inline uint8_t isBlocked(uint8_t channel)
{
	return (readADC(channel) < ADC_THRESHOLD) ? 1 : 0;
}
 
void resetTemp()
{
	uint8_t i = 0;
	
	for(i = 0; i< FRAME; i++)
	{
		VerticalSensor[i] = 0;
		HorizontalSensor[i] = 0;
	}
}

/*
This fuction saves the coordinate if there is any 
Then resets the sensor data
*/
bool Coordinate_Handler(void)
{
	uint32_t i;
	bool found_x = false;
	bool found_y = false;
	uint32_t temp_x = 0;
	uint32_t temp_y = 0;
	
	// Find X
	for(i = 0; i < FRAME; i++)
	{
		if(HorizontalSensor[i] == 1)
		{
			temp_x = i;
			found_x = true;
			HorizontalSensor[i] = 0; // Clear for next time
			break;
		}
	}
	
	// Find Y
	for(i = 0; i < FRAME; i++)
	{
		if(VerticalSensor[i] == 1)
		{
			temp_y = i;
			found_y = true;
			VerticalSensor[i] = 0; // Clear for next time
			break;
		}
	}
	
	//only save if full coordinate found
	if(found_x && found_y && GridMemory < MEMORY)
	{
		X_AXIS[GridMemory] = temp_x;
		Y_AXIS[GridMemory] = temp_y;
		GridMemory++;
		return true;
	}
	
	resetTemp();
	
	return false;
}


/*
Returns the most recently saved location 
*/
void MostRecentCoordinate(uint8_t *x, uint8_t *y)
{
	//return if Grid is Empty
	if(GridMemory == 0)
	{
		*x = 100; 
		*y = 100;
		return;
	}
	
	*x = X_AXIS[GridMemory - 1];
	*y = Y_AXIS[GridMemory - 1];
	
	return;
}

void ResetGrid(void)
{
	uint32_t i;
	
	//clearing saved Grid
	for(i = 0; i < MEMORY; i++)
	{
		X_AXIS[i] = 0;
		Y_AXIS[i] = 0;
	}

	GridMemory = 0;
	
	return;
}

//this functions clears everything that was saved
void ClearEverything(void)
{
	uint32_t i;
	
	//clearing sensor data
	for(i = 0; i < FRAME; i++)
	{
		HorizontalSensor[i] = 0;
		VerticalSensor[i] = 0;
	}
	CurrentIRLED = 0;
	
	ResetGrid();
	return;
}

int main(void){
	
	//Setting System Clock to be 100MHZ
	//AHB 100MHZ, APB1 25MHZ, APB2 50MHZ
	initSystemClockTo100Mhz();
	
	initGPIO(); 
	initADC();
	
	CurrentIRLED = 0;
	GridMemory = 0;
	
	//resetting shift register 
	GPIOA->BSRR = CLEAR_PIN << 16;
	delayMs(1);
	GPIOA->BSRR = CLEAR_PIN;
	
	
	SPI1_init();
	tft_init();
	fill_screen(WHITE);
	draw_rectangle_border(TFT_WIDTH/2, TFT_HEIGHT/2, 124, 124, 2, BLACK); //124 is height & width, 2 is border thickness
	
	
	uint16_t rect_x = 4;      //offsets and size modifier for blocks
	uint16_t rect_y = 20;
	uint16_t block_size = 12;
	
	uint8_t x;
	uint8_t y;
	uint8_t i;
	
	CurrentIRLED = 0;
	
	while(1)
	{		
		//pulsing clock 
		for(i = 0; i < CurrentIRLED + 1; i++)
		{
			
			if(i == 0) GPIOC->BSRR = DATA_PIN; 
			else GPIOC->BSRR = DATA_PIN << 16;
			__NOP();__NOP();__NOP(); //dealying by 30ns

			
			GPIOA->BSRR = CLOCK_PIN;
			__NOP();__NOP(); //dealying by 20ns
			GPIOA->BSRR = CLOCK_PIN << 16;
			__NOP();__NOP(); //dealying by 20ns
		}
		
		if(CurrentIRLED >= 7)
		{
			//pulsing clock 
			GPIOA->BSRR = CLOCK_PIN;
			__NOP();__NOP(); //dealying by 20ns
			GPIOA->BSRR = CLOCK_PIN << 16;
			__NOP();__NOP(); //dealying by 20ns
		}
		
		if(CurrentIRLED >= 14)
		{
			//pulsing clock 
			GPIOA->BSRR = CLOCK_PIN;
			__NOP();__NOP();__NOP();__NOP();__NOP();__NOP(); //dealying by 20ns
			GPIOA->BSRR = CLOCK_PIN << 16;
			__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();//dealying by 20ns
		}		
		
		
		//delay to let the Sensor get the correct input
		delayUs(50);
		
		if (CurrentIRLED >= FRAME)
		{
			HorizontalSensor[CurrentIRLED - FRAME] = isBlocked(HORIZ_CHANNEL);
		}
		else
		{
			VerticalSensor[CurrentIRLED] = isBlocked(VERT_CHANNEL);
		}		
		
		

		//reseting shift registers
		GPIOA->BSRR = CLEAR_PIN << 16;
		__NOP();__NOP(); //dealying by 20ns
		GPIOA->BSRR = CLEAR_PIN;
		__NOP();__NOP(); __NOP();__NOP(); __NOP();__NOP(); __NOP();__NOP(); __NOP();__NOP(); 
		__NOP();__NOP(); __NOP();__NOP(); __NOP();__NOP(); __NOP();__NOP(); 
		

		
		// draining the gpio 
		if(CurrentIRLED <= 9)
		{
			//Set PC0 (Vertical) to Output 
			GPIOC->MODER = (GPIOC->MODER & ~(0x3 << 0)) | (0x1 << 0); 

			//writing 0 to PCO drain capacitance 
			GPIOC->BSRR = (1 << 16); 
		
			delayUs(50);

			// Set PC0 back to ANALOG MODE for  ADC
			GPIOC->MODER |= (0x3 << 0);
			GPIOC->PUPDR &= ~(0x3 << 0); // no pull-up/pull-down
			__NOP(); __NOP();  
		}
		
		
		if(CurrentIRLED > 9)
		{
			//Set PC1 (Horizontal) Output
			GPIOC->MODER = (GPIOC->MODER & ~(0x3 << 2)) | (0x1 << 2); 

			//writing 1 to PC1 drain capacitance 
			GPIOC->BSRR = (1 << 17); 
		
			delayUs(50);

			// Set PC1 back to ANALOG MODE for  ADC
			GPIOC->MODER |= (0x3 << 2);
			GPIOC->PUPDR &= ~(0x3 << 2); // no pull-up/pull-down
			__NOP(); __NOP(); 
		}
		
		
		
		CurrentIRLED++;
		if((CurrentIRLED >= (FRAME * 2)))
		{
			CurrentIRLED = 0; //reset and go back to first LED
			
			//if memory full reset the display
			if(GridMemory >= MEMORY - 1)
			{
				ResetGrid();
			}

			new_data_available = Coordinate_Handler(); //saves the coordinate;
			if(new_data_available)
			{
			
				MostRecentCoordinate(&x, &y);
				draw_block(rect_x, rect_y, block_size, x, y, BLACK);
			
				new_data_available = false;
			}	
		}
	}
}