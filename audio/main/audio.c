#include "audio.h"
#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_hal.h"
#include "audio_pipeline.h"
#include "board.h"
#include "es7210.h"
#include "es8311.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "esp_wifi.h"
#include "esxxx_common.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "http.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "nvs_flash.h"
#include "periph_button.h"

static const char *TAG = "AUDIO";

int player_volume = 70; // percent
int mic_gain = GAIN_37_5DB;

esp_periph_set_handle_t    periph_set;
audio_board_handle_t       board_handle;
audio_pipeline_handle_t    adc_pipeline, dac_pipeline;
audio_element_handle_t     adc_i2s, dac_i2s, http_up_stream, http_down_stream;
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
	// ADC pipeline: ADC -> HTTP POST /audio
	ESP_LOGI(TAG, "Create ADC pipeline");
	audio_pipeline_cfg_t adc_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	adc_pipeline = audio_pipeline_init(&adc_pipeline_cfg);
	mem_assert(adc_pipeline);

	ESP_LOGI(TAG, "Create ADC i2s stream");
	i2s_stream_cfg_t adc_i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(
	    (i2s_port_t)0, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_STREAM_READER, AUDIO_CHANNELS);
	adc_i2s_cfg.type = AUDIO_STREAM_READER;
	adc_i2s_cfg.out_rb_size = 64 * 1024;
	adc_i2s = i2s_stream_init(&adc_i2s_cfg);
	i2s_stream_set_clk(adc_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
	audio_pipeline_register(adc_pipeline, adc_i2s, "adc");

	ESP_LOGI(TAG, "Create HTTP upload stream");
	http_stream_cfg_t http_up_cfg = HTTP_STREAM_CFG_DEFAULT();
	http_up_cfg.type = AUDIO_STREAM_WRITER;
	http_up_cfg.event_handle = _http_up_stream_event_handle;
	http_up_stream = http_stream_init(&http_up_cfg);
	audio_pipeline_register(adc_pipeline, http_up_stream, "http-up");

	// DAC pipeline: HTTP GET /audio -> DAC
/*         ESP_LOGI(TAG, "Create DAC pipeline");
 *         audio_pipeline_cfg_t dac_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
 *         dac_pipeline = audio_pipeline_init(&dac_pipeline_cfg);
 *         mem_assert(dac_pipeline);
 *
 *         ESP_LOGI(TAG, "Create DAC i2s stream");
 *         i2s_stream_cfg_t dac_i2s_cfg = I2S_STREAM_CFG_DEFAULT();
 *         dac_i2s_cfg.type = AUDIO_STREAM_WRITER;
 *         dac_i2s = i2s_stream_init(&dac_i2s_cfg);
 *         i2s_stream_set_clk(dac_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
 *         audio_pipeline_register(dac_pipeline, dac_i2s, "dac");
 *
 *         ESP_LOGI(TAG, "Create HTTP download stream");
 *         http_stream_cfg_t http_down_cfg = HTTP_STREAM_CFG_DEFAULT();
 *         http_down_cfg.type = AUDIO_STREAM_READER;
 *         http_down_cfg.event_handle = _http_down_stream_event_handle;
 *         http_down_stream = http_stream_init(&http_down_cfg);
 *         audio_pipeline_register(dac_pipeline, http_down_stream, "http-down"); */

	// Link pipelines
	ESP_LOGI(TAG, "Link ADC pipeline");
	const char *adc_link_tag[2] = {"adc", "http-up"};
	audio_pipeline_link(adc_pipeline, &adc_link_tag[0], 2);

	/* ESP_LOGI(TAG, "Link DAC pipeline");
	 * const char *dac_link_tag[2] = {"http-down", "dac"};
	 * audio_pipeline_link(dac_pipeline, &dac_link_tag[0], 2); */

	ESP_LOGI(TAG, "Set up event listener");
	audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
	evt = audio_event_iface_init(&evt_cfg);

	ESP_LOGI(TAG, "Listening event from all elements of pipeline");
	audio_pipeline_set_listener(adc_pipeline, evt);
	/* audio_pipeline_set_listener(dac_pipeline, evt); */

	ESP_LOGI(TAG, "Listening event from peripherals");
	audio_event_iface_set_listener(esp_periph_set_get_event_iface(periph_set), evt);

	return ESP_OK;
}

esp_err_t audio_deinit(void) {
	ESP_LOGI(TAG, "Stop ADC pipeline");
	audio_pipeline_stop(adc_pipeline);
	audio_pipeline_wait_for_stop(adc_pipeline);
	audio_pipeline_terminate(adc_pipeline);
	audio_pipeline_unregister(adc_pipeline, adc_i2s);

	ESP_LOGI(TAG, "Stop DAC pipeline");
	audio_pipeline_stop(dac_pipeline);
	audio_pipeline_wait_for_stop(dac_pipeline);
	audio_pipeline_terminate(dac_pipeline);
	audio_pipeline_unregister(dac_pipeline, dac_i2s);

	/* Terminate the pipeline before removing the listener */
	audio_pipeline_remove_listener(adc_pipeline);

	/* Make sure audio_pipeline_remove_listener is called before destroying
	 * event_iface */
	audio_event_iface_destroy(evt);

	/* Release all resources */
	audio_pipeline_deinit(adc_pipeline);
	audio_pipeline_deinit(dac_pipeline);
	audio_element_deinit(adc_i2s);
	audio_element_deinit(http_up_stream);
	audio_element_deinit(dac_i2s);
	audio_element_deinit(http_down_stream);
	return ESP_OK;
}
