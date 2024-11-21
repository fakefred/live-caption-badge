/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _AUDIO_BOARD_DEFINITION_H_
#define _AUDIO_BOARD_DEFINITION_H_

#include "audio_hal.h"

#define SDCARD_OPEN_FILE_NUM_MAX  5

#define BOARD_PA_GAIN             (10) /* Power amplifier gain defined by board (dB) */

#define SDCARD_PWR_CTRL             -1
#define ESP_SD_PIN_CLK              -1
#define ESP_SD_PIN_CMD              -1
#define ESP_SD_PIN_D0               -1
#define ESP_SD_PIN_D1               -1
#define ESP_SD_PIN_D2               -1
#define ESP_SD_PIN_D3               -1
#define ESP_SD_PIN_D4               -1
#define ESP_SD_PIN_D5               -1
#define ESP_SD_PIN_D6               -1
#define ESP_SD_PIN_D7               -1
#define ESP_SD_PIN_CD               -1
#define ESP_SD_PIN_WP               -1

#define INPUT_KEY_USER_ID_1 42
#define INPUT_KEY_USER_ID_2 41
#define INPUT_KEY_USER_ID_3 40

#define BUTTON_ID_1 42
#define BUTTON_ID_2 41
#define BUTTON_ID_3 40

extern audio_hal_func_t AUDIO_CODEC_ES7210_DEFAULT_HANDLE;
extern audio_hal_func_t AUDIO_CODEC_ES8311_DEFAULT_HANDLE;

#define AUDIO_CODEC_DEFAULT_CONFIG()                                           \
	{                                                                      \
	    .adc_input = AUDIO_HAL_ADC_INPUT_LINE1,                            \
	    .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,                            \
	    .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,                           \
	    .i2s_iface =                                                       \
	        {                                                              \
	            .mode = AUDIO_HAL_MODE_SLAVE,                              \
	            .fmt = AUDIO_HAL_I2S_NORMAL,                               \
	            .samples = AUDIO_HAL_48K_SAMPLES,                          \
	            .bits = AUDIO_HAL_BIT_LENGTH_16BITS,                       \
	        },                                                             \
	};

#define INPUT_KEY_NUM     3             /* You need to define the number of input buttons on your board */

#define INPUT_KEY_DEFAULT_INFO()                                               \
	{                                                                      \
	    {                                                                  \
	        .type = PERIPH_ID_BTN,                                         \
	        .user_id = INPUT_KEY_USER_ID_1,                                \
	        .act_id = BUTTON_ID_1,                                         \
	    },                                                                 \
	    {                                                                  \
	        .type = PERIPH_ID_BTN,                                         \
	        .user_id = INPUT_KEY_USER_ID_2,                                \
	        .act_id = BUTTON_ID_2,                                         \
	    },                                                                 \
	    {                                                                  \
	        .type = PERIPH_ID_BTN,                                         \
	        .user_id = INPUT_KEY_USER_ID_3,                                \
	        .act_id = BUTTON_ID_3,                                         \
	    },                                                                 \
	}

#endif
