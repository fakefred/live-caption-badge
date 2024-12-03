#include "audio.h"
#include "http_client.h"

#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_hal.h"
#include "audio_pipeline.h"
#include "board.h"
#include "es7210.h"
#include "es8311.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "freertos/task.h"
#include "http_stream.h"
#include "i2s_stream.h"

static const char *TAG = "AUDIO";

int player_volume = 70; // percent
int mic_gain = GAIN_37_5DB;

esp_periph_set_handle_t    periph_set;
audio_board_handle_t       board_handle;
audio_pipeline_handle_t    tx_pipeline, rx_pipeline;
audio_element_handle_t     adc_i2s, tx_http;
audio_element_handle_t     dac_i2s, rx_http;
audio_event_iface_handle_t evt;

esp_err_t audio_init(void) {
	// Initialize ES8311 and ES7210
	ESP_LOGI(TAG, "Start audio codec chips");
	board_handle = audio_board_init();
	audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE,
	                     AUDIO_HAL_CTRL_START);
	audio_hal_ctrl_codec(board_handle->adc_hal, AUDIO_HAL_CODEC_MODE_ENCODE,
	                     AUDIO_HAL_CTRL_START);

	ESP_ERROR_CHECK(audio_hal_set_volume(board_handle->audio_hal, player_volume));
	ESP_ERROR_CHECK(audio_hal_get_volume(board_handle->audio_hal, &player_volume));
	ESP_LOGW(TAG, "DAC volume is: %d", player_volume);

	ESP_ERROR_CHECK(es7210_adc_set_gain(ES7210_INPUT_MIC1, GAIN_37_5DB));
	ESP_ERROR_CHECK(es7210_adc_get_gain(ES7210_INPUT_MIC1, &mic_gain));
	ESP_LOGW(TAG, "Mic gain is: %d", mic_gain);

	// Create pipelines
	// TX pipelines: Vosk & Peer TX
	ESP_LOGI(TAG, "Create Vosk pipeline");
	audio_pipeline_cfg_t tx_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	tx_pipeline = audio_pipeline_init(&tx_pipeline_cfg);
	mem_assert(tx_pipeline);

	ESP_LOGI(TAG, "Create ADC i2s stream");
	i2s_stream_cfg_t adc_i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(
	    (i2s_port_t)0, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_STREAM_READER, AUDIO_CHANNELS);
	adc_i2s_cfg.type = AUDIO_STREAM_READER;
	adc_i2s_cfg.out_rb_size = 64 * 1024;
	adc_i2s = i2s_stream_init(&adc_i2s_cfg);
	i2s_stream_set_clk(adc_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
	audio_pipeline_register(tx_pipeline, adc_i2s, "adc");

	ESP_LOGI(TAG, "Create HTTP Vosk stream");
	http_stream_cfg_t tx_http_cfg = HTTP_STREAM_CFG_DEFAULT();
	tx_http_cfg.type = AUDIO_STREAM_WRITER;
	tx_http_cfg.event_handle = _http_up_stream_event_handle;
	tx_http = http_stream_init(&tx_http_cfg);
	audio_element_set_uri(tx_http, CONFIG_SERVER_URI);
	audio_pipeline_register(tx_pipeline, tx_http, "tx-http");

	// RX pipeline: Peer RX
	ESP_LOGI(TAG, "Create Peer RX pipeline");
	audio_pipeline_cfg_t rx_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	rx_pipeline = audio_pipeline_init(&rx_pipeline_cfg);
	mem_assert(rx_pipeline);

	ESP_LOGI(TAG, "Create DAC i2s stream");
	i2s_stream_cfg_t dac_i2s_cfg = I2S_STREAM_CFG_DEFAULT();
	dac_i2s_cfg.type = AUDIO_STREAM_WRITER;
	dac_i2s = i2s_stream_init(&dac_i2s_cfg);
	i2s_stream_set_clk(dac_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
	audio_pipeline_register(rx_pipeline, dac_i2s, "dac");

	ESP_LOGI(TAG, "Create HTTP download stream");
	http_stream_cfg_t rx_http_cfg = HTTP_STREAM_CFG_DEFAULT();
	rx_http_cfg.type = AUDIO_STREAM_READER;
	rx_http_cfg.event_handle = _http_down_stream_event_handle;
	rx_http = http_stream_init(&rx_http_cfg);
	audio_element_set_uri(rx_http, CONFIG_SERVER_URI);
	audio_pipeline_register(rx_pipeline, rx_http, "rx-http");

	/*
	 * Link pipelines:
	 *
	 * adc_i2s ------- tx_http
	 *
	 */
	ESP_LOGI(TAG, "Link TX pipelines");
	const char *vosk_link_tag[2] = {"adc", "tx-http"};
	audio_pipeline_link(tx_pipeline, vosk_link_tag, 2);

	/*
	 * rx_http ------- dac_i2s
	 */
	ESP_LOGI(TAG, "Link RX pipeline");
	const char *rx_link_tag[2] = {"rx-http", "dac"};
	audio_pipeline_link(rx_pipeline, rx_link_tag, 2);

	ESP_LOGI(TAG, "Set up event listener");
	audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
	evt = audio_event_iface_init(&evt_cfg);

	ESP_LOGI(TAG, "Listening event from all elements of pipeline");
	/* audio_pipeline_set_listener(vosk_pipeline, evt); */
	/* audio_pipeline_set_listener(dac_pipeline, evt); */

	ESP_LOGI(TAG, "Listening event from peripherals");
	audio_event_iface_set_listener(esp_periph_set_get_event_iface(periph_set), evt);

	return ESP_OK;
}

esp_err_t audio_deinit(void) {
	ESP_LOGI(TAG, "Stop ADC pipeline");
	audio_pipeline_stop(tx_pipeline);
	audio_pipeline_wait_for_stop(tx_pipeline);
	audio_pipeline_terminate(tx_pipeline);
	audio_pipeline_unregister(tx_pipeline, adc_i2s);

	ESP_LOGI(TAG, "Stop DAC pipeline");
	audio_pipeline_stop(rx_pipeline);
	audio_pipeline_wait_for_stop(rx_pipeline);
	audio_pipeline_terminate(rx_pipeline);
	audio_pipeline_unregister(rx_pipeline, dac_i2s);

	/* Terminate the pipeline before removing the listener */
	/* audio_pipeline_remove_listener(vosk_pipeline); */

	/* Make sure audio_pipeline_remove_listener is called before destroying
	 * event_iface */
	audio_event_iface_destroy(evt);

	/* Release all resources */
	audio_pipeline_deinit(tx_pipeline);
	audio_pipeline_deinit(rx_pipeline);
	audio_element_deinit(adc_i2s);
	audio_element_deinit(tx_http);
	audio_element_deinit(rx_http);
	audio_element_deinit(dac_i2s);
	return ESP_OK;
}
