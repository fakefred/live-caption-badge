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

	// Initialize the SPI bus
	ret = spi_bus_initialize(SPI_HOST, &spi_bus_cfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(ret);

	// Attach the Slave device to the SPI bus
	ret = spi_bus_add_device(SPI_HOST, &spi_dev_cfg, &spi);
	ESP_ERROR_CHECK(ret);
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
