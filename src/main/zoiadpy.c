#include <stdio.h>
#include <assert.h>
#include "driver/spi_slave.h"  
#include "driver/gpio.h"     
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>


#define CMD_SETCONTRAST 0x81
#define CMD_DISPLAYALLON_RESUME 0xA4
#define CMD_DISPLAYALLON 0xA5
#define CMD_NORMALDISPLAY 0xA6
#define CMD_INVERTDISPLAY 0xA7
#define CMD_DISPLAYOFF 0xAE
#define CMD_DISPLAYON 0xAF
#define CMD_SETDISPLAYOFFSET 0xD3
#define CMD_SETCOMPINS 0xDA
#define CMD_SETVCOMDETECT 0xDB
#define CMD_SETDISPLAYCLOCKDIV 0xD5
#define CMD_SETPRECHARGE 0xD9
#define CMD_SETMULTIPLEX 0xA8
#define CMD_SETLOWCOLUMN 0x00
#define CMD_SETHIGHCOLUMN 0x10
#define CMD_SETSTARTLINE 0x40
#define CMD_MEMORYMODE 0x20
#define CMD_COLUMNADDR 0x21
#define CMD_PAGEADDR 0x22
#define CMD_COMSCANINC 0xC0
#define CMD_COMSCANDEC 0xC8
#define CMD_SEGREMAP 0xA0
#define CMD_CHARGEPUMP 0x8D
#define CMD_EXTERNALVCC 0x1
#define CMD_SWITCHCAPVCC 0x2
#define CMD_ACTIVATE_SCROLL 0x2F
#define CMD_DEACTIVATE_SCROLL 0x2E
#define CMD_SET_VERTICAL_SCROLL_AREA 0xA3
#define CMD_RIGHT_HORIZONTAL_SCROLL 0x26
#define CMD_LEFT_HORIZONTAL_SCROLL 0x27
#define CMD_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define CMD_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A

#define LCD_W 128
#define LCD_H 64

enum lcd_state {
   LCD_STATE_IDLE,
   LCD_STATE_CMD,
   LCD_STATE_DATA,
};

struct lcd {
   enum lcd_state state;
   uint8_t cmd;
   bool onoff;
   uint16_t ptr;
   uint8_t fb[LCD_H * (LCD_W / 8)];
};


void lcd_init(struct lcd *lcd)
{
   memset(lcd, 0, sizeof(*lcd));
   lcd->state = LCD_STATE_IDLE;
   lcd->onoff = true;
   lcd->ptr = 0;
   memset(lcd->fb, 0, sizeof(lcd->fb));
}


static void lcd_debug(struct lcd *lcd, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   vfprintf(stderr, fmt, args);
   va_end(args);
}


void dpy_pset(struct lcd *lcd, uint8_t x, uint8_t y, uint8_t v)
{
	if(x < 128 && y < 64) {

		uint16_t o = (y/8) * LCD_W + x;
		uint8_t b = y & 0x07;

		if(v) {
			lcd->fb[o] |= (1<<b);
		} else {
			lcd->fb[o] &= ~(1<<b);
		}
	}
}

void lcd_flush(struct lcd *lcd)
{
   printf("\e[H");
   for(int y=0; y < LCD_H; y++) {
      for(int x=0; x < LCD_W; x++) {
         uint16_t o = (y/8) * LCD_W + x;
         uint8_t b = y & 0x07;

         if(lcd->fb[o] & (1<<b)) {
            putchar('#');
         } else {
            putchar(' ');
         }
      }
      printf("\n");
   }

   usleep(17 * 1000);
}

void lcd_handle_byte(struct lcd *lcd, uint8_t b)
{

   if(lcd->state == LCD_STATE_IDLE) {
      switch(b) {
         case CMD_DISPLAYOFF:
            lcd_debug(lcd, "display off\n");
            lcd->onoff = false;
            break;
         case CMD_DISPLAYON:
            lcd_debug(lcd, "display on\n");
            lcd->onoff = true;
            lcd->state = LCD_STATE_DATA;
            break;
         case CMD_SETSTARTLINE:
            lcd_debug(lcd, "set start line: 0x%02X\n", b);
            break;
         case CMD_DISPLAYALLON_RESUME:
            lcd_debug(lcd, "display all on resume\n");
            break;
         case CMD_NORMALDISPLAY:
            lcd_debug(lcd, "normal display\n");
            break;
         case CMD_SETDISPLAYCLOCKDIV:
         case CMD_SETMULTIPLEX:
         case CMD_SETDISPLAYOFFSET:
         case CMD_CHARGEPUMP:
         case CMD_MEMORYMODE:
         case CMD_SEGREMAP:
         case CMD_SETCOMPINS:
         case CMD_SETCONTRAST:
         case CMD_SETPRECHARGE:
         case CMD_SETVCOMDETECT:
            lcd->state = LCD_STATE_CMD;
            lcd->cmd = b;
            break;
         default:
            lcd_debug(lcd, "unhandled command: 0x%02X\n", b);
      }
   }

   else if(lcd->state == LCD_STATE_CMD) {
      switch(lcd->cmd) {
         case CMD_SETDISPLAYCLOCKDIV:
            lcd_debug(lcd, "set display clock div: 0x%02X\n", b);
            lcd->state = LCD_STATE_IDLE;
            break;
         case CMD_SETMULTIPLEX:
            lcd_debug(lcd, "set multiplex: 0x%02X\n", b);
            lcd->state = LCD_STATE_IDLE;
            break;
         case CMD_SETDISPLAYOFFSET:
            lcd_debug(lcd, "set display offset: 0x%02X\n", b);
            lcd->state = LCD_STATE_IDLE;
            break;
         case CMD_SETSTARTLINE:
            lcd_debug(lcd, "set start line: 0x%02X\n", b);
            lcd->state = LCD_STATE_IDLE;
            break;
         case CMD_CHARGEPUMP:
            lcd_debug(lcd, "set charge pump: 0x%02X\n", b);
            lcd_debug(lcd, "charge pump %s\n", (b & CMD_EXTERNALVCC) ? "external" : "switchcap");
            break;
         case CMD_MEMORYMODE:
            lcd_debug(lcd, "set memory mode: 0x%02X\n", b);
            if(b & 0x01) {
               lcd_debug(lcd, "memory mode: horizontal addressing\n");
            } else if(b & 0x02) {
               lcd_debug(lcd, "memory mode: vertical addressing\n");
            } else {
               lcd_debug(lcd, "memory mode: page addressing\n");
            }
            lcd->state = LCD_STATE_IDLE;
            break;
         case CMD_SEGREMAP:
            lcd_debug(lcd, "set segment remap: 0x%02X\n", b);
            if(b & 0x01) {
               lcd_debug(lcd, "segment remap: column 127 to SEG0\n");
            } else {
               lcd_debug(lcd, "segment remap: column 0 to SEG0\n");
            }
            lcd->state = LCD_STATE_IDLE;
            break;
         case CMD_SETCOMPINS:
            lcd_debug(lcd, "set com pins: 0x%02X\n", b);
            if(b & 0x02) {
               lcd_debug(lcd, "com pins: alternative configuration\n");
            } else {
               lcd_debug(lcd, "com pins: normal configuration\n");
            }
            lcd->state = LCD_STATE_IDLE;
            break;
         case CMD_SETCONTRAST:
            lcd_debug(lcd, "set contrast: 0x%02X\n", b);
            if(b == 0) {
               lcd_debug(lcd, "contrast: minimum\n");
            } else if(b == 0xFF) {
               lcd_debug(lcd, "contrast: maximum\n");
            } else {
               lcd_debug(lcd, "contrast: %d\n", b);
            }
            lcd->state = LCD_STATE_IDLE;
            break;
         case CMD_SETPRECHARGE:
            lcd_debug(lcd, "set precharge: 0x%02X\n", b);
            if(b == 0) {
               lcd_debug(lcd, "precharge: minimum\n");
            } else if(b == 0xFF) {
               lcd_debug(lcd, "precharge: maximum\n");
            } else {
               lcd_debug(lcd, "precharge: %d\n", b);
            }
            lcd->state = LCD_STATE_IDLE;
            break;
         case CMD_SETVCOMDETECT:
            lcd_debug(lcd, "set VCOM detect: 0x%02X\n", b);
            if(b == 0) {
               lcd_debug(lcd, "VCOM detect: minimum\n");
            } else if(b == 0xFF) {
               lcd_debug(lcd, "VCOM detect: maximum\n");
            } else {
               lcd_debug(lcd, "VCOM detect: %d\n", b);
            }
            lcd->state = LCD_STATE_IDLE;
            break;
         default:
            lcd_debug(lcd, "unhandled command data: 0x%02X, 0x%02X\n", lcd->cmd, b);
      }
      lcd->state = LCD_STATE_IDLE;
   }

   else if(lcd->state == LCD_STATE_DATA) {
      if(b == CMD_DISPLAYON) {
         lcd->ptr = 0;
      }

      lcd->fb[lcd->ptr] = b;
      lcd->ptr++;

      if(lcd->ptr >= sizeof(lcd->fb)) {
         lcd_flush(lcd);
         lcd->ptr = 0; // reset pointer to avoid overflow
      }
   }
}



#define XFER_SIZE 1024*8

static void spi_init(void)
{

   spi_bus_config_t buscfg = {
      .mosi_io_num = 27,
      .miso_io_num = -1,
      .sclk_io_num = 26,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = XFER_SIZE,
   };

   spi_slave_interface_config_t slvcfg = {
      .mode = 0,
      .spics_io_num = 25,
      .queue_size = 3,
      .flags = 0,
   };

   int ret = spi_slave_initialize(SPI3_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
   assert(ret == ESP_OK);

}

static uint8_t buf_tx[XFER_SIZE];
static uint8_t buf_rx[XFER_SIZE];

void app_main(void)
{
   printf("Hello, world!\n");
   spi_init();

   struct lcd lcd;
   lcd_init(&lcd);
   lcd.state = LCD_STATE_DATA;

   for(;;) {

      spi_slave_transaction_t t = {
         .length = XFER_SIZE,
         .tx_buffer = buf_tx,
         .rx_buffer = buf_rx,
      };

      int ret = spi_slave_transmit(SPI3_HOST, &t, portMAX_DELAY);
      if(ret == ESP_OK) {
         if(1) {
            for(size_t i=0; i<t.trans_len/8; i++) {
               lcd_handle_byte(&lcd, buf_rx[i]);
            }
         }
      }
   }



}

// vi: ts=3 sw=3 et
