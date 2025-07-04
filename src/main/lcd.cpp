
#include <stdio.h>
#include <string.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lcd.hpp"

gpio_num_t k_gpio_reset = GPIO_NUM_16;
gpio_num_t k_gpio_dc = GPIO_NUM_17;
gpio_num_t k_gpio_backlight = GPIO_NUM_15;

#define LCD_H_RES 480
#define LCD_V_RES 320
#define LCD_BUF_LINES 16



#define ILI9488_CMD_SLEEP_OUT                       0x11
#define ILI9488_CMD_POSITIVE_GAMMA_CORRECTION       0xE0
#define ILI9488_CMD_NEGATIVE_GAMMA_CORRECTION       0xE1
#define ILI9488_CMD_POWER_CONTROL_1                 0xC0
#define ILI9488_CMD_POWER_CONTROL_2                 0xC1
#define ILI9488_CMD_VCOM_CONTROL_1                  0xC5
#define ILI9488_CMD_MEMORY_ACCESS_CONTROL           0x36
#define ILI9488_CMD_COLMOD_PIXEL_FORMAT_SET         0x3A
#define ILI9488_CMD_INTERFACE_MODE_CONTROL          0xB0
#define ILI9488_CMD_FRAME_RATE_CONTROL_NORMAL       0xB1
#define ILI9488_CMD_DISPLAY_INVERSION_CONTROL       0xB4
#define ILI9488_CMD_DISPLAY_FUNCTION_CONTROL        0xB6
#define ILI9488_CMD_SET_IMAGE_FUNCTION              0xE9
#define ILI9488_CMD_WRITE_CTRL_DISPLAY              0x53
#define ILI9488_CMD_WRITE_DISPLAY_BRIGHTNESS        0x51
#define ILI9488_CMD_ADJUST_CONTROL_3                0xF7
#define ILI9488_CMD_DISPLAY_ON                      0x29
#define ILI9488_CMD_COLUMN_ADDRESS_SET              0x2A
#define ILI9488_CMD_PAGE_ADDRESS_SET                0x2B
#define ILI9488_CMD_MEMORY_WRITE                    0x2C


// portrait: 0x48
// portrait_inverted: 0x88
// landscape: 0x28
// landscape_inverted: 0xE8

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;


DRAM_ATTR static const lcd_init_cmd_t lcd_init_cmds[] = { 
   { ILI9488_CMD_SLEEP_OUT, { 0x00 }, 0x80 },
   { ILI9488_CMD_POSITIVE_GAMMA_CORRECTION, { 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F }, 15 },
   { ILI9488_CMD_NEGATIVE_GAMMA_CORRECTION, { 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F }, 15 },
   { ILI9488_CMD_POWER_CONTROL_1, { 0x17, 0x15 }, 2 },
   { ILI9488_CMD_POWER_CONTROL_2, { 0x41 }, 1 },
   { ILI9488_CMD_VCOM_CONTROL_1, { 0x00, 0x12, 0x80 }, 3 },
   { ILI9488_CMD_MEMORY_ACCESS_CONTROL, { 0x28 }, 1 },
   { ILI9488_CMD_COLMOD_PIXEL_FORMAT_SET, { 0x61 }, 1 },
   { ILI9488_CMD_INTERFACE_MODE_CONTROL, { 0x00 }, 1 },
   { ILI9488_CMD_FRAME_RATE_CONTROL_NORMAL, { 0xA0 }, 1 },
   { ILI9488_CMD_DISPLAY_INVERSION_CONTROL, { 0x02 }, 1 },
   { ILI9488_CMD_DISPLAY_FUNCTION_CONTROL, { 0x02, 0x02 }, 2 },
   { ILI9488_CMD_SET_IMAGE_FUNCTION, { 0x00 }, 1 },
   { ILI9488_CMD_WRITE_CTRL_DISPLAY, { 0x28 }, 1 },
   { ILI9488_CMD_WRITE_DISPLAY_BRIGHTNESS, { 0x7F }, 1 },
   { ILI9488_CMD_ADJUST_CONTROL_3, { 0xA9, 0x51, 0x2C, 0x02 }, 4 },
   { ILI9488_CMD_DISPLAY_ON, { 0x00 }, 0x80 },
   { 0, { 0 }, 0xff },
 };





void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(k_gpio_dc, dc);
}


void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void*)0;
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
}


void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
   if(len > 0) {
      spi_transaction_t t;
      memset(&t, 0, sizeof(t));
      t.length = len*8;
      t.tx_buffer = data;
      t.user = (void*)1;
      ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
   }
}


void Lcd::disp_flush(const Area *area,  uint8_t *color_p)
{
   spi_transaction_t (&t)[6] = m_spi_transaction;
   memset(t, 0, sizeof(t));

   t[0].flags = SPI_TRANS_USE_TXDATA;
   t[0].length = 1 * 8;
   t[0].tx_data[0] = ILI9488_CMD_COLUMN_ADDRESS_SET;
   t[0].user = (void*)0;

   t[1].flags = SPI_TRANS_USE_TXDATA;
   t[1].length = 4 * 8;
   t[1].tx_data[0] = area->x >> 8;
   t[1].tx_data[1] = area->x >> 0;
   t[1].tx_data[2] = (area->x + area->w - 1) >> 8;
   t[1].tx_data[3] = (area->x + area->w - 1) >> 0;
   t[1].user = (void*)1;

   t[2].flags = SPI_TRANS_USE_TXDATA;
   t[2].length = 1 * 8;
   t[2].tx_data[0] = ILI9488_CMD_PAGE_ADDRESS_SET;
   t[2].user = (void*)0;

   t[3].flags = SPI_TRANS_USE_TXDATA;
   t[3].length = 4 * 8;
   t[3].tx_data[0] = area->y >> 8;
   t[3].tx_data[1] = area->y >> 0;
   t[3].tx_data[2] = (area->y + area->h - 1) >> 8;
   t[3].tx_data[3] = (area->y + area->h - 1) >> 0;
   t[3].user = (void*)1;

   t[4].flags = SPI_TRANS_USE_TXDATA;
   t[4].length = 1 * 8;
   t[4].tx_data[0] = ILI9488_CMD_MEMORY_WRITE;
   t[4].user = (void*)0;

   t[5].length = (area->w * area->h) * 4 - 32; // why?
   t[5].tx_buffer = color_p;
   t[5].user = (void*)1;

   for(int i=0; i<6; i++) {
      ESP_ERROR_CHECK(spi_device_polling_transmit(m_spidev, &t[i]));
   }

}


Lcd::Lcd(gpio_num_t gpio_miso, gpio_num_t gpio_mosi, gpio_num_t gpio_sclk)
   : m_npending(0)
   , m_gpio_miso(gpio_miso)
   , m_gpio_mosi(gpio_mosi)
        , m_gpio_sclk(gpio_sclk)
{
}


void Lcd::init()
{
   spi_bus_config_t buscfg;
   memset(&buscfg, 0, sizeof(buscfg));
   buscfg.miso_io_num = m_gpio_miso;
   buscfg.mosi_io_num = m_gpio_mosi;
   buscfg.sclk_io_num = m_gpio_sclk;
   buscfg.quadwp_io_num = -1;
   buscfg.quadhd_io_num = -1;
   buscfg.max_transfer_sz = 4096 * 8;
   ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

   spi_device_interface_config_t devcfg;
   memset(&devcfg, 0, sizeof(devcfg));
   devcfg.clock_speed_hz = 40 * 1000 * 1000;
   devcfg.mode = 0;
   devcfg.spics_io_num = GPIO_NUM_4;
   devcfg.queue_size = 7;
   devcfg.pre_cb = lcd_spi_pre_transfer_callback;
   devcfg.flags = SPI_DEVICE_HALFDUPLEX;

   ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &m_spidev));

   gpio_config_t io_conf = {};
   io_conf.mode = GPIO_MODE_OUTPUT;
   io_conf.pin_bit_mask = (1ULL << k_gpio_reset) 
                        | (1ULL << k_gpio_dc)
                        | (1ULL << k_gpio_backlight);
   gpio_config(&io_conf);
   gpio_set_level(k_gpio_backlight, 1);

   gpio_set_level(k_gpio_reset, 0);
   vTaskDelay(100 / portTICK_PERIOD_MS);
   gpio_set_level(k_gpio_reset, 1);
   vTaskDelay(100 / portTICK_PERIOD_MS);

   int cmd = 0;
   while (lcd_init_cmds[cmd].databytes != 0xff) {
      lcd_cmd(m_spidev, lcd_init_cmds[cmd].cmd, false);
      lcd_data(m_spidev, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes & 0x1F);
      if (lcd_init_cmds[cmd].databytes & 0x80) {
         vTaskDelay(100 / portTICK_PERIOD_MS);
      }
      cmd++;
   }

   uint16_t black[480];
   memset(black, 0, sizeof(black));
   Area area;
   for(int y=0; y<320; y++) {
      area.x = 0;
      area.y = 0;
      area.w = 480;
      area.h = 1;
      disp_flush(&area, (uint8_t *)black);
   }

}


// vi: ts=3 sw=3 et

