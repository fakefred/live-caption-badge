#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdio.h>

#define SPI_TRANS_SIZE 2048 // 2048 bytes per transacton

#define SPI_HOST SPI2_HOST

void spi_init(void);
void spi_write_byte(uint8_t byte);
void spi_write_bytes(uint8_t *data, uint16_t len);

#define SPI_TAG "SPI"
