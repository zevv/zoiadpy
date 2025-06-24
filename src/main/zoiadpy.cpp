#include <stdio.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>

#include "driver/spi_slave.h"  
#include "driver/gpio.h"     

#include "lcd.hpp"



static void spi_init(void)
{

   spi_bus_config_t buscfg{};
   buscfg.mosi_io_num = 27;
   buscfg.miso_io_num = -1;
   buscfg.sclk_io_num = 26;
   buscfg.quadwp_io_num = -1;
   buscfg.quadhd_io_num = -1;
   buscfg.max_transfer_sz = 1024 * 8;

   spi_slave_interface_config_t slvcfg{};
   slvcfg.mode = 0;
   slvcfg.spics_io_num = 25;
   slvcfg.queue_size = 3;
   slvcfg.flags = 0;

   int ret = spi_slave_initialize(SPI3_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
   assert(ret == ESP_OK);

}


#define OLED_W 128
#define OLED_H 64
#define SCALE 3

static void copy_screen2(Lcd *lcd, uint8_t *buf_rx)
{

   for(int y=0; y<OLED_H; y++) {

      uint8_t line[OLED_W];
      for (int x=0; x<OLED_W; x++) {
         uint16_t o = (y/8) * OLED_W + x;
         uint8_t b = y & 0x07;
         line[x] = buf_rx[o] & (1 << b);
      }

      static uint8_t fb_line[OLED_W * SCALE];
      uint8_t *p = fb_line;
      for(int x=0; x<OLED_W*SCALE; x+=2) {
         *p = 0;
         if(line[(x+0)/SCALE]) *p |= (7 << 3);
         if(line[(x+1)/SCALE]) *p |= (7 << 0);
         p++;
      }
      
      for(int dy=0; dy<SCALE; dy++) {
         lv_area_t area;
         int sy = y * SCALE + dy;
         area.x1 = 48;
         area.x2 = OLED_W * SCALE - 1 + 48;
         area.y1 = sy + 64;
         area.y2 = sy + 64;
         lcd->disp_flush(&area, fb_line);
      }
   }
}




static double hirestime(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec / 1e9;
}



static uint8_t buf_rx[1024];

void start(void)
{
   spi_init();

   Lcd *lcd = new Lcd(GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_18);
   lcd->init();

   for(;;) {

      spi_slave_transaction_t t{};
      t.length = sizeof(buf_rx) * 8;
      t.tx_buffer = nullptr;
      t.rx_buffer = buf_rx;

      int ret = spi_slave_transmit(SPI3_HOST, &t, portMAX_DELAY);
      if(ret == ESP_OK) {

         if(t.trans_len/8 == 1024) {
            double t1 = hirestime();
            copy_screen2(lcd, buf_rx);
            double t2 = hirestime();
            printf("%d %.3f\n", t.trans_len/8, 1.0/(t2-t1));
         } else {
            printf("%d\n", t.trans_len/8);
         }
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
