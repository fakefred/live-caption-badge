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
#include "http_client.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "nvs_flash.h"
#include "periph_button.h"
#include "ringbuf.h"

static const char *TAG = "AUDIO";

int player_volume = 70; // percent
int mic_gain = GAIN_37_5DB;

esp_periph_set_handle_t    periph_set;
audio_board_handle_t       board_handle;
audio_pipeline_handle_t    vosk_pipeline, peer_tx_pipeline;
audio_pipeline_handle_t    peer_rx_pipeline;
audio_element_handle_t     adc_i2s, vosk_http_stream, peer_tx_http_stream;
audio_element_handle_t     dac_i2s, peer_rx_http_stream;
ringbuf_handle_t           peer_rx_ringbuf;
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
	audio_pipeline_cfg_t vosk_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	vosk_pipeline = audio_pipeline_init(&vosk_pipeline_cfg);
	mem_assert(vosk_pipeline);

	ESP_LOGI(TAG, "Create Peer TX pipeline");
	audio_pipeline_cfg_t peer_tx_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	peer_tx_pipeline = audio_pipeline_init(&peer_tx_pipeline_cfg);
	mem_assert(peer_tx_pipeline);

	ESP_LOGI(TAG, "Create ADC i2s stream");
	i2s_stream_cfg_t adc_i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(
	    (i2s_port_t)0, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_STREAM_READER, AUDIO_CHANNELS);
	adc_i2s_cfg.type = AUDIO_STREAM_READER;
	adc_i2s_cfg.out_rb_size = 64 * 1024;
	adc_i2s = i2s_stream_init(&adc_i2s_cfg);
	i2s_stream_set_clk(adc_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
	audio_pipeline_register(vosk_pipeline, adc_i2s, "adc");

	ESP_LOGI(TAG, "Create HTTP Vosk stream");
	http_stream_cfg_t vosk_http_cfg = HTTP_STREAM_CFG_DEFAULT();
	vosk_http_cfg.type = AUDIO_STREAM_WRITER;
	vosk_http_cfg.event_handle = _http_up_stream_event_handle;
	vosk_http_stream = http_stream_init(&vosk_http_cfg);
	audio_element_set_uri(vosk_http_stream, CONFIG_SERVER_URI);
	audio_pipeline_register(vosk_pipeline, vosk_http_stream, "http-vosk");

	ESP_LOGI(TAG, "Create HTTP Peer TX stream");
	http_stream_cfg_t peer_tx_http_cfg = HTTP_STREAM_CFG_DEFAULT();
	peer_tx_http_cfg.type = AUDIO_STREAM_WRITER;
	peer_tx_http_cfg.event_handle = _http_up_stream_event_handle;
	peer_tx_http_stream = http_stream_init(&peer_tx_http_cfg);
	audio_element_set_uri(peer_tx_http_stream, "http://192.168.227.130:80/audio"); // TODO
	/* audio_pipeline_register(peer_tx_pipeline, peer_tx_http_stream, "http-peer-tx"); */
	audio_pipeline_register(vosk_pipeline, peer_tx_http_stream, "http-peer-tx");

	// RX pipeline: Peer RX
        ESP_LOGI(TAG, "Create Peer RX pipeline");
	audio_pipeline_cfg_t peer_rx_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	peer_rx_pipeline = audio_pipeline_init(&peer_rx_pipeline_cfg);
	mem_assert(peer_rx_pipeline);

	ESP_LOGI(TAG, "Create DAC i2s stream");
	i2s_stream_cfg_t dac_i2s_cfg = I2S_STREAM_CFG_DEFAULT();
	dac_i2s_cfg.type = AUDIO_STREAM_WRITER;
	dac_i2s = i2s_stream_init(&dac_i2s_cfg);
	i2s_stream_set_clk(dac_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
	audio_pipeline_register(peer_rx_pipeline, dac_i2s, "dac");

	/* ESP_LOGI(TAG, "Create HTTP download stream");
	 * http_stream_cfg_t peer_rx_http_cfg = HTTP_STREAM_CFG_DEFAULT();
	 * peer_rx_http_cfg.type = AUDIO_STREAM_READER;
	 * peer_rx_http_cfg.event_handle = _http_down_stream_event_handle;
	 * peer_rx_http_stream = http_stream_init(&peer_rx_http_cfg);
         * audio_pipeline_register(peer_rx_pipeline, peer_rx_http_stream, "http-peer-rx"); */

	/*
	 * Link pipelines:
	 *
	 * adc_i2s _______ vosk_http_stream
	 *            |
	 *            |___ peer_tx_http_stream
	 *
	 */
	ESP_LOGI(TAG, "Link TX pipelines");
	/* const char *vosk_link_tag[2] = {"adc", "http-vosk"}; */
	const char *vosk_link_tag[2] = {"adc", "http-peer-tx"};
	audio_pipeline_link(vosk_pipeline, vosk_link_tag, 2);
	
	/* const char *peer_tx_link_tag[1] = {"http-peer-tx"}; */
	/* audio_pipeline_link(peer_tx_pipeline, peer_tx_link_tag, 1); */

	/* ringbuf_handle_t peer_tx_http_rb = audio_element_get_input_ringbuf(peer_tx_http_stream); */
	/* audio_element_set_multi_output_ringbuf(adc_i2s, peer_tx_http_rb, 0); */

	/*
	 * peer_rx_http_stream ------- dac_i2s
	 */
	ESP_LOGI(TAG, "Link RX pipeline");
	const char *peer_rx_link_tag[1] = {"dac"};
	audio_pipeline_link(peer_rx_pipeline, peer_rx_link_tag, 1);

	peer_rx_ringbuf = rb_create(64 * 1024, 1);
	audio_element_set_input_ringbuf(dac_i2s, peer_rx_ringbuf);

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
	audio_pipeline_stop(vosk_pipeline);
	audio_pipeline_wait_for_stop(vosk_pipeline);
	audio_pipeline_terminate(vosk_pipeline);
	audio_pipeline_unregister(vosk_pipeline, adc_i2s);

	ESP_LOGI(TAG, "Stop DAC pipeline");
	audio_pipeline_stop(peer_rx_pipeline);
	audio_pipeline_wait_for_stop(peer_rx_pipeline);
	audio_pipeline_terminate(peer_rx_pipeline);
	audio_pipeline_unregister(peer_rx_pipeline, dac_i2s);

	/* Terminate the pipeline before removing the listener */
	audio_pipeline_remove_listener(vosk_pipeline);

	/* Make sure audio_pipeline_remove_listener is called before destroying
	 * event_iface */
	audio_event_iface_destroy(evt);

	/* Release all resources */
	audio_pipeline_deinit(vosk_pipeline);
	audio_pipeline_deinit(peer_rx_pipeline);
	audio_element_deinit(adc_i2s);
	audio_element_deinit(vosk_http_stream);
	audio_element_deinit(dac_i2s);
	audio_element_deinit(peer_tx_http_stream);
	return ESP_OK;
}
