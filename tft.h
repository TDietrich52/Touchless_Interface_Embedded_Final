#ifndef TFT_H
#define TFT_H

#include "stm32f446xx.h"
#include <stdint.h>
#include "clock.h"
// https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf
// data sheet for board. has spi commands an operations

// colors
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define MAGENTA 0xF81F

// display info
#define TFT_WIDTH   128
#define TFT_HEIGHT  160
#define X_OFFSET    0
#define Y_OFFSET    0

// macros for readability
#define CS_LOW()   (GPIOB->BSRR = (1 << (6+16)))
#define CS_HIGH()  (GPIOB->BSRR = (1 << 6))
#define DC_LOW()   (GPIOA->BSRR = (1 << (0+16)))
#define DC_HIGH()  (GPIOA->BSRR = (1 << 0))


//spi functs. init and byte write
void SPI1_init(void); // also enables used gpio pins
void SPI1_write(uint8_t data);

//tft commands
void tft_init(void); 
void tft_command(uint8_t cmd); //send command byte
void tft_data(uint8_t data); //send data byte
void tft_data_bytes(uint8_t* buff, uint32_t len); // send multiple bytes of data
void tft_cmd_data(uint8_t cmd, uint8_t *data, int len); // command followed by data bytes

// display specific commands
void tft_set_addr(uint16_t x, uint16_t y); // sets drawing address
void draw_pixel(uint16_t x, uint16_t y, uint16_t color); // draws single pixel
void fill_screen(uint16_t color); // fills screen
void fill_screen_fast(uint16_t color); // also fills screen. uses buffer to write chunks and be "faster", but it ended up not really being faster. 

void draw_rectangle_border(uint16_t x_center, uint16_t y_center, uint16_t width, uint16_t height, uint16_t thickness, uint16_t color); // draw rectangle outline
void fill_rectangle_inside(uint16_t x_center, uint16_t y_center, uint16_t width, uint16_t height, uint16_t color); // SLOW. USE OTHER FUNCTION INSTEAD
void fill_rectangle_inside_fast(uint16_t x_center, uint16_t y_center, uint16_t width, uint16_t height, uint16_t color); //also fills inside, but coded closer to what fill_screen is like. faster that way.

void draw_block(uint16_t rect_x, uint16_t rect_y, uint16_t block_size, uint16_t x_idx, uint16_t y_idx, uint16_t color); // fill in small, but bigger chunk. (also slow function, but because 6x6 blocks are so small, no big deal)
void draw_block_fast(uint16_t rect_x, uint16_t rect_y, uint16_t block_size, uint16_t x_idx, uint16_t y_idx, uint16_t color); // also coded like fill_screen. better version of draw_block basically.

#endif // TFT_H