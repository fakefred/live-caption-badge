/*
 * This file was adapted from Krutarth Purohit's spi_protocol:
 * https://github.com/krutarthpurohit/spi_protocol
 *
 * Copyright (c) 2024 Krutarth Purohit
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "SPI.h"
#include "DEV_Config.h"

esp_err_t           ret;
spi_device_handle_t spi;

void spi_init(void) {
	spi_bus_config_t spi_bus_cfg = {
	    // Set SPI pins
	    .miso_io_num = EPD_MISO_PIN,
	    .mosi_io_num = EPD_MOSI_PIN,
	    .sclk_io_num = EPD_SCK_PIN,

	    // NOT using quad spi
	    .quadhd_io_num = -1,
	    .quadwp_io_num = -1,

	    // Max SPI transfer size
	    // 4095 bytes is the max size of data that can be sent because of hardware limitations
	    .max_transfer_sz = 512 * 8};

	spi_device_interface_config_t spi_dev_cfg = {
	    // configure device_structure
	    .clock_speed_hz = 2 * 1000 * 1000, // Clock out at 12 MHz
	    .mode = 0,                         // SPI mode 0: CPOL:-0 and CPHA:-0
	    .spics_io_num = EPD_CS_PIN,
	    .queue_size = 7, // We want to be able to queue 7 transactions at a time
	};

	if (spi == NULL) {
		// Initialize the SPI bus
		ret = spi_bus_initialize(SPI_HOST, &spi_bus_cfg, SPI_DMA_CH_AUTO);
		ESP_ERROR_CHECK(ret);

		// Attach the Slave device to the SPI bus
		ret = spi_bus_add_device(SPI_HOST, &spi_dev_cfg, &spi);
		ESP_ERROR_CHECK(ret);
	}
}

void spi_write_byte(uint8_t byte) {
	spi_transaction_t trans = {.flags = SPI_TRANS_USE_TXDATA, .tx_data = {byte}, .length = 8};

	ret = spi_device_transmit(spi, &trans);
	if (ret != ESP_OK) {
		ESP_LOGE(SPI_TAG, "SPI Write Operation Failed :(");
	} else {
		ESP_LOGI(SPI_TAG, "Sent a byte! :D");
	}
}

void spi_write_bytes(uint8_t *data, uint16_t len) {
	spi_transaction_t trans = {.tx_buffer = data, .length = len * 8};

	ret = spi_device_transmit(spi, &trans);
	if (ret != ESP_OK) {
		ESP_LOGE(SPI_TAG, "SPI Write Operation Failed :(");
	} else {
		ESP_LOGI(SPI_TAG, "Sent %d bytes! :D", len);
	}
}
