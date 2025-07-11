
#pragma once

#include "driver/spi_master.h"  
#include "driver/gpio.h"     

struct Area {
	int x, y;
	int w, h;
};

class Lcd {

public:
	Lcd(gpio_num_t gpio_miso, gpio_num_t gpio_mosi, gpio_num_t gpio_sclk);
	void init();
	void disp_flush(const Area *area,  uint8_t *color_p);

private:
	
	int m_npending;
	spi_device_handle_t m_spidev;
	spi_transaction_t m_spi_transaction[6];

	gpio_num_t m_gpio_miso;
	gpio_num_t m_gpio_mosi;
	gpio_num_t m_gpio_sclk;
};


