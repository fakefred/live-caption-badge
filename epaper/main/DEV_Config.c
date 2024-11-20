/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-02-19
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "DEV_Config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "hal/gpio_types.h"

#define TAG "DEV_Config"

void GPIO_Config(void) {
	gpio_config_t input_config = {
	    .pin_bit_mask = 1ULL << EPD_BUSY_PIN,
	    .mode = GPIO_MODE_INPUT,
	    .pull_up_en = GPIO_PULLUP_DISABLE,
	    .pull_down_en = GPIO_PULLDOWN_DISABLE,
	    .intr_type = GPIO_INTR_POSEDGE,
	};
	gpio_config(&input_config);

	gpio_config_t output_config = {
	    .pin_bit_mask = (1ULL << EPD_RST_PIN) | (1ULL << EPD_DC_PIN) |
	                    (1ULL << EPD_SCK_PIN) | (1ULL << EPD_MOSI_PIN) |
	                    (1ULL << EPD_CS_PIN) | (1ULL << EPD_PWR_PIN),
	    .mode = GPIO_MODE_OUTPUT,
	    .pull_up_en = GPIO_PULLUP_DISABLE,
	    .pull_down_en = GPIO_PULLDOWN_DISABLE,
	    .intr_type = GPIO_INTR_DISABLE,
	};
	gpio_config(&output_config);

	gpio_set_level(EPD_CS_PIN, HIGH);
	gpio_set_level(EPD_SCK_PIN, LOW);
	gpio_set_level(EPD_PWR_PIN, LOW);
	DEV_Delay_ms(500);
	gpio_set_level(EPD_PWR_PIN, HIGH);
}

/******************************************************************************
function:	Module Initialize, the BCM2835 library and initialize the pins,
SPI protocol parameter: Info:
******************************************************************************/
UBYTE DEV_Module_Init(void) {
	GPIO_Config();
	spi_init();
	return 0;
}
