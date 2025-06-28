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

static const int buf_rx_size = 1024;


static void spi_init(void)
{

   spi_bus_config_t buscfg{};
   buscfg.mosi_io_num = 27;
   buscfg.miso_io_num = -1;
   buscfg.sclk_io_num = 26;
   buscfg.quadwp_io_num = -1;
   buscfg.quadhd_io_num = -1;
   buscfg.max_transfer_sz = buf_rx_size * 8;

   spi_slave_interface_config_t slvcfg{};
   slvcfg.mode = 0;
   slvcfg.spics_io_num = 25;
   slvcfg.queue_size = 3;
   slvcfg.flags = 0;

   int ret = spi_slave_initialize(SPI3_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
   assert(ret == ESP_OK);

}


static double hirestime(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec / 1e9;
}


static void copy_screen2(Lcd *lcd, uint8_t *buf_rx)
{
   static const int src_w = 128;
   static const int src_h = 64;
   static const int dst_w = 480;
   static const int dst_h = 320;
   static const int scale_y = dst_h / src_h;

   double t1 = hirestime();

   for(int y=0; y<src_h; y++) {

      // Decode one horizontal OLED line

      static uint8_t fb_src[src_w];
      for (int x=0; x<src_w; x++) {
         int o = (y / 8) * src_w + x;
         int b = y % 8;
         fb_src[x] = buf_rx[o] & (1 << b);
      }

      // Upscale to multiple LCD lines

      static uint8_t fb_dst[scale_y][dst_w / 2];
      int di = 0;
      int x = 0;
      while(x < dst_w) {
         uint8_t v = 0;
         if(fb_src[x++ * src_w / dst_w]) v |= (7 << 3);
         if(fb_src[x++ * src_w / dst_w]) v |= (7 << 0);
         for(int dy=0; dy<scale_y; dy++) {
            fb_dst[dy][di] = v;
         }
         di++;
      }

      Area area;
      area.x = 0;
      area.y = y * scale_y;
      area.w = dst_w;
      area.h = scale_y;
      lcd->disp_flush(&area, fb_dst[0]);
   }

   double t2 = hirestime();

   if(0) {
      printf("copy_screen2: %.3f\n", t2 - t1);
   }
}




static QueueHandle_t queue; 


void task_lcd(void *arg)
{
   Lcd *lcd = (Lcd *)arg;

   for(;;) {
      uint8_t buf[buf_rx_size];
      int ticks = pdMS_TO_TICKS(33);
      if(xQueueReceive(queue, buf, ticks)) {
         copy_screen2(lcd, buf);
      }
   }
}


void start(void)
{
   spi_init();

   queue = xQueueCreate(10, buf_rx_size);

   Lcd *lcd = new Lcd(GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_18);
   lcd->init();

   xTaskCreate(task_lcd, "task_lcd", 4096, lcd, 5, nullptr);

   uint8_t *buf = (uint8_t *)heap_caps_malloc(buf_rx_size, MALLOC_CAP_DMA);

   for(;;) {

      spi_slave_transaction_t t{};
      t.length = buf_rx_size * 8;
      t.tx_buffer = nullptr;
      t.rx_buffer = buf;

      int ret = spi_slave_transmit(SPI3_HOST, &t, portMAX_DELAY);
      if(ret == ESP_OK) {
         if(t.trans_len/8 == buf_rx_size) {
            xQueueSendToFront(queue, buf, portMAX_DELAY);
         } else {
            printf("xrun %d\n", t.trans_len/8);
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
