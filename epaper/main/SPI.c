#include "SPI.h"

esp_err_t ret;
spi_device_handle_t spi;

void spi_init(void) {
    gpio_set_direction(PIN_SPI_CS, GPIO_MODE_OUTPUT); //CS Pin set to output mode

    spi_bus_config_t spi_bus_cfg = {
        //Set SPI pins
        .miso_io_num = PIN_SPI_MISO,
        .mosi_io_num = PIN_SPI_MOSI,
        .sclk_io_num = PIN_SPI_CLK,

        //NOT using quad spi
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,

        //Max SPI transfer size
        .max_transfer_sz = 512 * 8 // 4095 bytes is the max size of data that can be sent because of hardware limitations
    };

    spi_device_interface_config_t spi_dev_cfg = {
        // configure device_structure
        .clock_speed_hz = 2 * 1000 * 1000,                             // Clock out at 12 MHz
        .mode = 0,                                                      // SPI mode 0: CPOL:-0 and CPHA:-0
        .spics_io_num = PIN_SPI_CS,                                     // This field is used to specify the GPIO pin that is to be used as CS'
        .queue_size = 7,                                                // We want to be able to queue 7 transactions at a time
    };

    ret = spi_bus_initialize(ESP_HOST, &spi_bus_cfg, SPI_DMA_CH_AUTO);       // Initialize the SPI bus
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(ESP_HOST, &spi_dev_cfg, &spi);                  // Attach the Slave device to the SPI bus
    ESP_ERROR_CHECK(ret);
}

void spi_write_byte(uint8_t byte) {
    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .tx_data = {byte},
        .length = 8
    };

    ret = spi_device_transmit(spi, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(SPI_TAG, "SPI Write Operation Failed :(\n");
    }
    ESP_LOGI(SPI_TAG, "Sent a byte! :D\n\n");
}

void spi_write_bytes(uint8_t *data, uint16_t len) {
    spi_transaction_t trans = {
        .tx_buffer = data,
        .length = len * 8
    };

    ret = spi_device_transmit(spi, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(SPI_TAG, "SPI Write Operation Failed :(\n");
    }
    ESP_LOGI(SPI_TAG, "Sent %d bytes! :D\n\n", len);
}

void delay(uint16_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}