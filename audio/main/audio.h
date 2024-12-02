#pragma once

#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"
#include "board.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_peripherals.h"
#include "freertos/task.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "ringbuf.h"

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_BITS 16
#define AUDIO_CHANNELS 1

#define BUTTON_ID_1 42
#define BUTTON_ID_2 41
#define BUTTON_ID_3 40

extern esp_periph_set_handle_t    periph_set;
extern audio_board_handle_t       board_handle;
extern audio_pipeline_handle_t    tx_pipeline, rx_pipeline;
extern audio_element_handle_t     adc_i2s, tx_http;
extern audio_element_handle_t     dac_i2s, rx_http;
extern audio_event_iface_handle_t evt;

esp_err_t audio_init(void);
esp_err_t audio_deinit(void);
