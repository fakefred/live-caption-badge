#pragma once

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define SPI_TRANS_SIZE  2048        //2048 bytes per transacton

#define PIN_SPI_MISO    GPIO_NUM_39 //Wont really need this
#define PIN_SPI_MOSI    GPIO_NUM_18
#define PIN_SPI_CLK     GPIO_NUM_19
#define PIN_SPI_CS      GPIO_NUM_7

#define ESP_HOST        SPI3_HOST

void spi_init(void);
void spi_write_byte(uint8_t byte);
void spi_write_bytes(uint8_t *data, uint16_t len);
void delay(uint16_t ms);

#define SPI_TAG "SPI"