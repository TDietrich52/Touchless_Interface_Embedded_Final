#include "tft.h"


void CS_HIGH_safe(void) {
	while (SPI1->SR & SPI_SR_BSY) {}  
	GPIOB->BSRR = (1 << 6);           //deselect display
}


void DC_LOW_safe(void) {
	while (SPI1->SR & SPI_SR_BSY) {}  
	GPIOA->BSRR = (1 << (0+16));      // command mode
}


void DC_HIGH_safe(void) {
	while (SPI1->SR & SPI_SR_BSY) {}  
	GPIOA->BSRR = (1 << 0);           // data mode
}


void SPI1_init(void) {
	//clocks
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;// gpioa
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;// gpiob
	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;// spi1

	// pa5 = SCK, pa6 = MOSI
	GPIOA->MODER &= ~((3 << (5*2)) | (3 << (7*2)));
	GPIOA->MODER |=  ((2 << (5*2)) | (2 << (7*2))); //alternate function
	GPIOA->AFR[0] &= ~((0xF << (5*4)) | (0xF << (7*4)));
	GPIOA->AFR[0] |= ((5 << (5*4)) | (5 << (7*4))); // af5 for spi1

	// pa1 = rst output
	GPIOA->MODER &= ~(3 << (1*2));
	GPIOA->MODER |= (1 << (1*2));

	// pa0 = dc output
	GPIOA->MODER &= ~(3 << (0*2));
	GPIOA->MODER |= (1 << (0*2));

	// pb6 = cs output
	GPIOB->MODER &= ~(3 << (6*2));
	GPIOB->MODER |= (1 << (6*2));

	//defaults for those 3 pins
	GPIOA->BSRR = (1 << 1); // rst
	GPIOA->BSRR = (1 << 0); // dc
	GPIOB->BSRR = (1 << 6); // cs

	//spi1 setup
	SPI1->CR1 = 0;
	SPI1->CR1 |= SPI_CR1_MSTR; //master mode for board. we're never reading from display.
	SPI1->CR1 |= SPI_CR1_SSM;  // these lines ensure cs determines when to write to slave (chip managed in software, and selected internally)
	SPI1->CR1 |= SPI_CR1_SSI;  

	// prescaler /2 (25MhZ)
	SPI1->CR1 &= ~SPI_CR1_BR;
	// prescaler /4 (12.5 MHz)
	//SPI1->CR1 |= (1 << SPI_CR1_BR_Pos);	
	// prescaler /8 (6.25 Mhz)
	//SPI1->CR1 |= (2 << SPI_CR1_BR_Pos);
	// prescaler /16 (3.125 Mhz)
	//SPI1->CR1 |= (3 << SPI_CR1_BR_Pos);

	//set PA5 and PA7 to high speed output 
	GPIOA->OSPEEDR &= ~((3 << (5*2)) | (3 << (7*2)));
	GPIOA->OSPEEDR |=  ((2 << (5*2)) | (2 << (7*2)));

	SPI1->CR1 |= SPI_CR1_SPE;  // Enable SPI
}

void SPI1_write(uint8_t data) {
	while (!(SPI1->SR & SPI_SR_TXE)) {} // wait for empty buffer
	SPI1->DR = data;
}


void tft_command(uint8_t cmd) {
	CS_LOW();
	DC_LOW();       //command mode
	SPI1_write(cmd);
	CS_HIGH_safe();
}

void tft_data(uint8_t data) {
	CS_LOW();
	DC_HIGH();      // data mode
	SPI1_write(data);
	CS_HIGH_safe(); 
}

void tft_data_bytes(uint8_t* buff, uint32_t len) {
	CS_LOW();
	DC_HIGH();      
	for (uint32_t i = 0; i < len; i++) {
		SPI1_write(buff[i]); 
	}
	CS_HIGH_safe(); 
}

void tft_cmd_data(uint8_t cmd, uint8_t *data, int len) {
	CS_LOW();
	
	DC_LOW();          
	SPI1_write(cmd);
	DC_HIGH_safe();    
	
	for (int i = 0; i < len; i++) {
		SPI1_write(data[i]); 
	}
	CS_HIGH_safe();
}


void tft_init(void) {
	// reset
	GPIOA->BSRR = (1 << (1+16));
	delayMs(20);
	GPIOA->BSRR = (1 << 1);
	delayMs(20);

	CS_LOW();

	// SWRESET
	DC_LOW(); SPI1_write(0x01);   
	delayMs(100);                   

	// SLPOUT
	DC_LOW(); SPI1_write(0x11);   
	delayMs(100);

	// color mode
	DC_LOW(); SPI1_write(0x3A);   
	DC_HIGH_safe(); SPI1_write(0x05); 

	// DISPON
	DC_LOW_safe(); SPI1_write(0x29);  
	delayMs(100);

	CS_HIGH_safe();
}

void tft_set_addr(uint16_t x, uint16_t y) {
	CS_LOW();
	// column
	DC_LOW();            
	SPI1_write(0x2A);
	DC_HIGH_safe();      
	
	SPI1_write(0x00); 
	SPI1_write(x); //start
	SPI1_write(0x00); 
	SPI1_write(x); //end

	// row
	DC_LOW_safe();       
	SPI1_write(0x2B);
	DC_HIGH_safe();      
	
	SPI1_write(0x00); 
	SPI1_write(y);
	SPI1_write(0x00); 
	SPI1_write(y);

	// RAM write
	DC_LOW_safe();       
	SPI1_write(0x2C);
	DC_HIGH_safe();     
}

void draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
	if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;

	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;

	CS_LOW();

	DC_LOW();       SPI1_write(0x2A);
	DC_HIGH_safe(); SPI1_write(0x00); SPI1_write(x); SPI1_write(0x00); SPI1_write(x);
	DC_LOW_safe();  SPI1_write(0x2B);
	DC_HIGH_safe(); SPI1_write(0x00); SPI1_write(y); SPI1_write(0x00); SPI1_write(y);
	DC_LOW_safe();  SPI1_write(0x2C);
	DC_HIGH_safe();

	SPI1_write(hi);
	SPI1_write(lo);
	CS_HIGH_safe(); 
}

void fill_screen(uint16_t color) {
	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;

	CS_LOW();
	// Setup window
	DC_LOW();       SPI1_write(0x2A);
	DC_HIGH_safe(); SPI1_write(0x00); SPI1_write(0x00); SPI1_write(0x00); SPI1_write(127);
	DC_LOW_safe();  SPI1_write(0x2B);
	DC_HIGH_safe(); SPI1_write(0x00); SPI1_write(0x00); SPI1_write(0x00); SPI1_write(159);
	DC_LOW_safe();  SPI1_write(0x2C);
	DC_HIGH_safe();

	for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
		SPI1_write(hi);
		SPI1_write(lo);
	}
	CS_HIGH_safe();
}

void fill_screen_fast(uint16_t color) {
	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;
	uint8_t buf[32];
	for(int i=0;i<32;i+=2) { 
		buf[i]=hi; 
		buf[i+1]=lo; 
	}

	CS_LOW();
	// Setup window
	DC_LOW();       SPI1_write(0x2A);
	DC_HIGH_safe(); SPI1_write(0x00); SPI1_write(0x00); SPI1_write(0x00); SPI1_write(127);
	DC_LOW_safe();  SPI1_write(0x2B);
	DC_HIGH_safe(); SPI1_write(0x00); SPI1_write(0x00); SPI1_write(0x00); SPI1_write(159);
	DC_LOW_safe();  SPI1_write(0x2C);
	DC_HIGH_safe();


	for(int i=0; i < (TFT_WIDTH*TFT_HEIGHT)/16; i++) tft_data_bytes(buf, 32);

	CS_HIGH_safe();
}

void draw_rectangle_border(uint16_t x_center, uint16_t y_center, uint16_t width, uint16_t height, uint16_t thickness, uint16_t color) {
	int x0 = x_center - width/2;
	int y0 = y_center - height/2;
	int x1 = x_center + width/2 - 1;
	int y1 = y_center + height/2 - 1;

	for(int t=0; t<thickness; t++){
			for(int x=x0;x<=x1;x++){ 
				draw_pixel(x,y0+t,color); 
				draw_pixel(x,y1-t,color); 
			}
			for(int y=y0;y<=y1;y++){ 
				draw_pixel(x0+t,y,color); 
				draw_pixel(x1-t,y,color); 
			}
	}
}

void fill_rectangle_inside(uint16_t x_center, uint16_t y_center, uint16_t width, uint16_t height, uint16_t color) {
	int x0 = x_center - width/2;
	int y0 = y_center - height/2;
	int x1 = x_center + width/2 - 1;
	int y1 = y_center + height/2 - 1;

	for(int y=y0;y<=y1;y++) {
		for(int x=x0;x<=x1;x++) {
			draw_pixel(x,y,color);
		}
	}
}

void fill_rectangle_inside_fast(uint16_t x_center, uint16_t y_center, uint16_t width, uint16_t height, uint16_t color) {
	uint16_t x0 = x_center - width/2;
	uint16_t y0 = y_center - height/2;
	uint16_t x1 = x_center + width/2 - 1;
	uint16_t y1 = y_center + height/2 - 1;

	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;

	CS_LOW();
	
	// Window setup — same plain/_safe pattern as the other setup blocks
	DC_LOW();       SPI1_write(0x2A);
	DC_HIGH_safe(); SPI1_write(x0>>8); SPI1_write(x0&0xFF); SPI1_write(x1>>8); SPI1_write(x1&0xFF);
	DC_LOW_safe();  SPI1_write(0x2B);
	DC_HIGH_safe(); SPI1_write(y0>>8); SPI1_write(y0&0xFF); SPI1_write(y1>>8); SPI1_write(y1&0xFF);
	DC_LOW_safe();  SPI1_write(0x2C);
	DC_HIGH_safe();

	// Hot loop: DC pinned high, pipelining runs wide open.
	uint32_t total_pixels = (x1 - x0 + 1)*(y1 - y0 + 1);
	for(uint32_t i=0;i<total_pixels;i++){ 
		SPI1_write(hi); 
		SPI1_write(lo); 
	}
	
	CS_HIGH_safe();
}


void draw_block(uint16_t rect_x, uint16_t rect_y, uint16_t block_size, uint16_t x_idx, uint16_t y_idx, uint16_t color) {
	uint16_t y_flipped = x_idx;
	uint16_t x_flipped = 9 - y_idx;
	uint16_t start_x = rect_x + x_flipped * block_size;
	uint16_t start_y = rect_y + y_flipped * block_size;

	for(uint16_t dx=0;dx<block_size;dx++) {
		for(uint16_t dy=0;dy<block_size;dy++) {
			draw_pixel(start_x+dx,start_y+dy,color);
		}
	}
	
}

void draw_block_fast(uint16_t rect_x, uint16_t rect_y, uint16_t block_size, uint16_t x_idx, uint16_t y_idx, uint16_t color) {
	uint16_t y_flipped = 19 - x_idx;
	uint16_t x_flipped = 19 - y_idx;
	uint16_t start_x = rect_x + x_flipped * block_size;
	uint16_t start_y = rect_y + y_flipped * block_size;
	uint16_t end_x = start_x + block_size - 1;
	uint16_t end_y = start_y + block_size - 1;

	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;

	CS_LOW();
	// Window setup — plain/_safe pattern
	DC_LOW();       SPI1_write(0x2A);
	DC_HIGH_safe(); SPI1_write(start_x>>8); SPI1_write(start_x&0xFF); SPI1_write(end_x>>8); SPI1_write(end_x&0xFF);
	DC_LOW_safe();  SPI1_write(0x2B);
	DC_HIGH_safe(); SPI1_write(start_y>>8); SPI1_write(start_y&0xFF); SPI1_write(end_y>>8); SPI1_write(end_y&0xFF);
	DC_LOW_safe();  SPI1_write(0x2C);
	DC_HIGH_safe();

	// Hot loop: DC stays high, no BSY stalls between bytes.
	uint32_t total_pixels = (end_x-start_x+1)*(end_y-start_y+1);
	for(uint32_t i=0;i<total_pixels;i++){ 
		SPI1_write(hi); SPI1_write(lo); 
	}
	
	CS_HIGH_safe();
}
