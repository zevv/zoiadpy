#include <stdio.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>

#include "driver/spi_slave.h"  
#include "driver/gpio.h"     

#include "lcd.hpp"

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

#define OLED_W 128
#define OLED_H 64

enum oled_state {
   OLED_STATE_IDLE,
   OLED_STATE_CMD,
   OLED_STATE_DATA,
};

struct oled {
   enum oled_state state;
   uint8_t cmd;
   bool onoff;
   uint16_t ptr;
   uint8_t fb[OLED_H * (OLED_W / 8)];
};


void oled_init(struct oled *oled)
{
   memset(oled, 0, sizeof(*oled));
   oled->state = OLED_STATE_IDLE;
   oled->onoff = true;
   oled->ptr = 0;
   memset(oled->fb, 0, sizeof(oled->fb));
}


static void oled_debug(struct oled *oled, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   vfprintf(stderr, fmt, args);
   va_end(args);
}


void dpy_pset(struct oled *oled, uint8_t x, uint8_t y, uint8_t v)
{
	if(x < 128 && y < 64) {

		uint16_t o = (y/8) * OLED_W + x;
		uint8_t b = y & 0x07;

		if(v) {
			oled->fb[o] |= (1<<b);
		} else {
			oled->fb[o] &= ~(1<<b);
		}
	}
}

void oled_flush(struct oled *oled)
{
   printf("\e[H");
   for(int y=0; y < OLED_H; y++) {
      for(int x=0; x < OLED_W; x++) {
         uint16_t o = (y/8) * OLED_W + x;
         uint8_t b = y & 0x07;

         if(oled->fb[o] & (1<<b)) {
            putchar('#');
         } else {
            putchar(' ');
         }
      }
      printf("\n");
   }

   usleep(17 * 1000);
}

void oled_handle_byte(struct oled *oled, uint8_t b)
{

   if(oled->state == OLED_STATE_IDLE) {
      switch(b) {
         case CMD_DISPLAYOFF:
            oled_debug(oled, "display off\n");
            oled->onoff = false;
            break;
         case CMD_DISPLAYON:
            oled_debug(oled, "display on\n");
            oled->onoff = true;
            oled->state = OLED_STATE_DATA;
            break;
         case CMD_SETSTARTLINE:
            oled_debug(oled, "set start line: 0x%02X\n", b);
            break;
         case CMD_DISPLAYALLON_RESUME:
            oled_debug(oled, "display all on resume\n");
            break;
         case CMD_NORMALDISPLAY:
            oled_debug(oled, "normal display\n");
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
            oled->state = OLED_STATE_CMD;
            oled->cmd = b;
            break;
         default:
            oled_debug(oled, "unhandled command: 0x%02X\n", b);
      }
   }

   else if(oled->state == OLED_STATE_CMD) {
      switch(oled->cmd) {
         case CMD_SETDISPLAYCLOCKDIV:
            oled_debug(oled, "set display clock div: 0x%02X\n", b);
            oled->state = OLED_STATE_IDLE;
            break;
         case CMD_SETMULTIPLEX:
            oled_debug(oled, "set multiplex: 0x%02X\n", b);
            oled->state = OLED_STATE_IDLE;
            break;
         case CMD_SETDISPLAYOFFSET:
            oled_debug(oled, "set display offset: 0x%02X\n", b);
            oled->state = OLED_STATE_IDLE;
            break;
         case CMD_SETSTARTLINE:
            oled_debug(oled, "set start line: 0x%02X\n", b);
            oled->state = OLED_STATE_IDLE;
            break;
         case CMD_CHARGEPUMP:
            oled_debug(oled, "set charge pump: 0x%02X\n", b);
            oled_debug(oled, "charge pump %s\n", (b & CMD_EXTERNALVCC) ? "external" : "switchcap");
            break;
         case CMD_MEMORYMODE:
            oled_debug(oled, "set memory mode: 0x%02X\n", b);
            if(b & 0x01) {
               oled_debug(oled, "memory mode: horizontal addressing\n");
            } else if(b & 0x02) {
               oled_debug(oled, "memory mode: vertical addressing\n");
            } else {
               oled_debug(oled, "memory mode: page addressing\n");
            }
            oled->state = OLED_STATE_IDLE;
            break;
         case CMD_SEGREMAP:
            oled_debug(oled, "set segment remap: 0x%02X\n", b);
            if(b & 0x01) {
               oled_debug(oled, "segment remap: column 127 to SEG0\n");
            } else {
               oled_debug(oled, "segment remap: column 0 to SEG0\n");
            }
            oled->state = OLED_STATE_IDLE;
            break;
         case CMD_SETCOMPINS:
            oled_debug(oled, "set com pins: 0x%02X\n", b);
            if(b & 0x02) {
               oled_debug(oled, "com pins: alternative configuration\n");
            } else {
               oled_debug(oled, "com pins: normal configuration\n");
            }
            oled->state = OLED_STATE_IDLE;
            break;
         case CMD_SETCONTRAST:
            oled_debug(oled, "set contrast: 0x%02X\n", b);
            if(b == 0) {
               oled_debug(oled, "contrast: minimum\n");
            } else if(b == 0xFF) {
               oled_debug(oled, "contrast: maximum\n");
            } else {
               oled_debug(oled, "contrast: %d\n", b);
            }
            oled->state = OLED_STATE_IDLE;
            break;
         case CMD_SETPRECHARGE:
            oled_debug(oled, "set precharge: 0x%02X\n", b);
            if(b == 0) {
               oled_debug(oled, "precharge: minimum\n");
            } else if(b == 0xFF) {
               oled_debug(oled, "precharge: maximum\n");
            } else {
               oled_debug(oled, "precharge: %d\n", b);
            }
            oled->state = OLED_STATE_IDLE;
            break;
         case CMD_SETVCOMDETECT:
            oled_debug(oled, "set VCOM detect: 0x%02X\n", b);
            if(b == 0) {
               oled_debug(oled, "VCOM detect: minimum\n");
            } else if(b == 0xFF) {
               oled_debug(oled, "VCOM detect: maximum\n");
            } else {
               oled_debug(oled, "VCOM detect: %d\n", b);
            }
            oled->state = OLED_STATE_IDLE;
            break;
         default:
            oled_debug(oled, "unhandled command data: 0x%02X, 0x%02X\n", oled->cmd, b);
      }
      oled->state = OLED_STATE_IDLE;
   }

   else if(oled->state == OLED_STATE_DATA) {

      oled->fb[oled->ptr] = b;
      oled->ptr++;

      if(oled->ptr >= sizeof(oled->fb)) {
         //oled_flush(oled);
         oled->ptr = 0; // reset pointer to avoid overflow
      }
   }
}



#define XFER_SIZE 1024*8

static void spi_init(void)
{

   spi_bus_config_t buscfg{};
   buscfg.mosi_io_num = 27;
   buscfg.miso_io_num = -1;
   buscfg.sclk_io_num = 26;
   buscfg.quadwp_io_num = -1;
   buscfg.quadhd_io_num = -1;
   buscfg.max_transfer_sz = XFER_SIZE;

   spi_slave_interface_config_t slvcfg{};
   slvcfg.mode = 0;
   slvcfg.spics_io_num = 25;
   slvcfg.queue_size = 3;
   slvcfg.flags = 0;

   int ret = spi_slave_initialize(SPI3_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
   assert(ret == ESP_OK);

}

   
// buffer for one scaled line, 3bpp, - - R G B R G B

// buffer for one scaled line, 3bpp packed: 2 pixels per byte (3 bits each)
static void copy_screen2(Lcd *lcd, uint8_t *buf_rx, uint8_t *buf_rx_prev)
{
#define SCALE 3

    static uint8_t fb_line[OLED_W * SCALE / 2];

    for (int y = 0; y < OLED_H; y++) {
        //if (memcmp(&buf_rx[o], &buf_rx_prev[o], OLED_W) == 0)
        //    continue;

        memset(fb_line, 0, sizeof(fb_line));

        uint16_t o = (y / 8) * OLED_W;

        int sx = 0;
        for (int x = 0; x < OLED_W; x++) {
            uint16_t o = (y / 8) * OLED_W + x;
            uint8_t b = y & 0x07;
            bool pixel_on = buf_rx[o] & (1 << b);

            uint8_t color = pixel_on ? 0b111 : 0b000;

            for (int dx = 0; dx < SCALE; dx++) {
                fb_line[sx / 2] |= color << (sx & 1 ? 0 : 3);
                sx++;
            }
        }

        // Send line SCALE times for vertical scaling
        for (int dy = 0; dy < SCALE; dy++) {
            lv_area_t area;
            int sy = y * SCALE + dy;

            area.x1 = 48;
            area.x2 = 48 + OLED_W * SCALE - 1;
            area.y1 = 32 + sy;
            area.y2 = 32 + sy;

            lcd->disp_flush(nullptr, &area, fb_line);
        }
    }
}









static uint8_t buf_tx[XFER_SIZE];
static uint8_t buf_rx[XFER_SIZE];
static uint8_t buf_rx_prev[XFER_SIZE];

void start(void)
{
   printf("Hello, world!\n");
   spi_init();

   struct oled oled;
   oled_init(&oled);
   oled.state = OLED_STATE_DATA;

   Lcd *lcd = new Lcd(GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_18);
   lcd->init();


   for(;;) {

      spi_slave_transaction_t t{};
      t.length = XFER_SIZE;
      t.tx_buffer = buf_tx;
      t.rx_buffer = buf_rx;

      int ret = spi_slave_transmit(SPI3_HOST, &t, portMAX_DELAY);
      if(ret == ESP_OK) {
         printf("%d\n", t.trans_len/8);
         if(1) {
            for(size_t i=0; i<t.trans_len/8; i++) {
               oled_handle_byte(&oled, buf_rx[i]);
            }
         }
      }

      if(t.trans_len/8 == 1024) {
         copy_screen2(lcd, buf_rx, buf_rx_prev);
         memcpy(buf_rx_prev, buf_rx, sizeof(buf_rx_prev));
      }

   }
}

extern "C" {
void app_main()
{
   start();
}
}

// vi: ts=3 sw=3 et
