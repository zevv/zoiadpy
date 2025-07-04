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

// ideally buf_rx_count should be >1 to allow the SPI slave to have a nice
// pool of buffers, but i'm running into silly data corruption when running
// both SPI master and slaves at the same time. This might be related to
// https://github.com/espressif/esp-idf/issues/12354. For now keep to
// 1 rx_buffer: still not perfect but doable.

static const int buf_rx_count = 1;
static const int oled_w = 128;
static const int oled_h = 64;
static const int buf_rx_size = oled_w * oled_h / 8;


static void spi_init(void)
{
   spi_bus_config_t buscfg{};
   buscfg.mosi_io_num = 27;
   buscfg.miso_io_num = -1;
   buscfg.sclk_io_num = 26;
   buscfg.quadwp_io_num = -1;
   buscfg.quadhd_io_num = -1;
   buscfg.max_transfer_sz = 2048 * 8;

   spi_slave_interface_config_t slvcfg{};
   slvcfg.mode = 0;
   slvcfg.spics_io_num = 25;
   slvcfg.queue_size = buf_rx_count;
   slvcfg.flags = 0;

   int ret = spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
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
   Lcd *lcd = new Lcd(GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_18);
   lcd->init();

   static uint8_t buf[buf_rx_size];

   for(;;) {
      xQueueReceive(queue, buf, portMAX_DELAY);
      copy_screen2(lcd, buf);
   }
}


void start(void)
{

   queue = xQueueCreate(buf_rx_count, buf_rx_size);
   xTaskCreate(task_lcd, "task_lcd", 8192, nullptr, 5, nullptr);

   // init spi and fill up the buffer pool
   spi_init();
   spi_slave_transaction_t ts[buf_rx_count]{};
   for(int i=0; i<buf_rx_count; i++) {
      spi_slave_transaction_t *t = &ts[i];
      t->length = buf_rx_size * 8;
      t->rx_buffer = spi_bus_dma_memory_alloc(SPI2_HOST, buf_rx_size, 0);
      t->user = (void *)i;
      int ret = spi_slave_queue_trans(SPI2_HOST, t, portMAX_DELAY);
      if(ret != ESP_OK) printf("spi_slave_queue_trans error %d\n", ret);
   }

   // main loop: pump the spi buffers and send off the received data. ideally
   // the spi_slave_transaction_t itself should be sent off and get requeued
   // from there, but that doesn't seem to be allowed from a different task
   int i =0;
   for(;;) {
      spi_slave_transaction_t *t;
      int ret = spi_slave_get_trans_result(SPI2_HOST, &t, portMAX_DELAY);
      if(ret == ESP_OK) {
         if(t->trans_len/8 == buf_rx_size) {
            if(0) ((uint8_t *)(t->rx_buffer))[i++ & 7] |= 0x01;
            xQueueSendToFront(queue, t->rx_buffer, portMAX_DELAY);
         } else {
            printf("xrun %d\n", t->trans_len/8);
            usleep(10 * 1000);
         }
         ret = spi_slave_queue_trans(SPI2_HOST, t, portMAX_DELAY);
         if(ret != ESP_OK) printf("spi_slave_queue_trans error %d\n", ret);
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
