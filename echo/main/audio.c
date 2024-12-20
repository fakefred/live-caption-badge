/* Play wav file by audio pipeline
   with possibility to start, stop, pause and resume playback
   as well as adjust volume

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_hal.h"
#include "audio_pipeline.h"
#include "board.h"
#include "board_def.h"
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
#include "http_stream.h"
#include "i2s_stream.h"
#include "nvs_flash.h"
#include "periph_button.h"

static const char *TAG = "ECHO";

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_BITS 16
#define AUDIO_CHANNELS 1

#define BUTTON_ID_1 42
#define BUTTON_ID_2 41
#define BUTTON_ID_3 40

void app_main(void) {
	// Print verbose log except wifi
	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("wifi", ESP_LOG_ERROR);
	esp_log_level_set(TAG, ESP_LOG_INFO);

	// Initialize a bunch of system-level stuff
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_LOGI(TAG, "Initialize peripherals");
	esp_periph_config_t     periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
	esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

	audio_board_key_init(set);

	// Initialize ES8311 and ES7210
	ESP_LOGI(TAG, "Start audio codec chips");
	audio_board_handle_t board_handle = audio_board_init();
	audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE,
	                     AUDIO_HAL_CTRL_START);
	audio_hal_ctrl_codec(board_handle->adc_hal, AUDIO_HAL_CODEC_MODE_ENCODE,
			     AUDIO_HAL_CTRL_START);

	int player_volume, mic_gain;
	ESP_ERROR_CHECK(audio_hal_set_volume(board_handle->audio_hal, 100));
	audio_hal_get_volume(board_handle->audio_hal, &player_volume);
	ESP_LOGW(TAG, "DAC volume is: %d", player_volume);

	ESP_ERROR_CHECK(es7210_adc_get_gain(ES7210_INPUT_MIC1, &mic_gain));
	ESP_LOGW(TAG, "Mic gain was: %d", mic_gain);
	ESP_ERROR_CHECK(es7210_adc_set_gain(ES7210_INPUT_MIC1, GAIN_37_5DB));
	ESP_ERROR_CHECK(es7210_adc_get_gain(ES7210_INPUT_MIC1, &mic_gain));
	ESP_LOGW(TAG, "Mic gain is: %d", mic_gain);

	/* ADC */

	ESP_LOGI(TAG, "Create ADC pipeline");
	audio_pipeline_cfg_t adc_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	audio_pipeline_handle_t adc_pipeline = audio_pipeline_init(&adc_pipeline_cfg);
	mem_assert(adc_pipeline);

	ESP_LOGI(TAG, "Create ADC i2s stream");
	i2s_stream_cfg_t adc_i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(
	    (i2s_port_t)0, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_STREAM_READER, AUDIO_CHANNELS);
	adc_i2s_cfg.type = AUDIO_STREAM_READER;
	adc_i2s_cfg.out_rb_size = 64 * 1024;
	audio_element_handle_t adc_i2s = i2s_stream_init(&adc_i2s_cfg);
	i2s_stream_set_clk(adc_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
	audio_pipeline_register(adc_pipeline, adc_i2s, "adc");

	ESP_LOGI(TAG, "Create DAC i2s stream");
	i2s_stream_cfg_t dac_i2s_cfg = I2S_STREAM_CFG_DEFAULT();
	dac_i2s_cfg.type = AUDIO_STREAM_WRITER;
	audio_element_handle_t dac_i2s = i2s_stream_init(&dac_i2s_cfg);
	i2s_stream_set_clk(dac_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
	audio_pipeline_register(adc_pipeline, dac_i2s, "dac");

	ESP_LOGI(TAG, "Link ADC pipeline");
	const char *link_tag[2] = {"adc", "dac"};
	audio_pipeline_link(adc_pipeline, &link_tag[0], 2);

	ESP_LOGI(TAG, "Set up ADC event listener");
	audio_event_iface_cfg_t    evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
	audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

	ESP_LOGI(TAG, "Listening event from all elements of pipeline");
	audio_pipeline_set_listener(adc_pipeline, evt);

	ESP_LOGI(TAG, "Listening event from peripherals");
	audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

	while (1) {
		audio_event_iface_msg_t msg;
		esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
		if (ret != ESP_OK) {
			continue;
		}

		if (msg.source_type == PERIPH_ID_BUTTON && msg.cmd == PERIPH_BUTTON_PRESSED) {
			ESP_LOGI(TAG, "Button %d pressed", (int)msg.data);

			if ((int)msg.data == BUTTON_ID_1) {
				ESP_LOGI(TAG, "[ * ] Button 1");
				ESP_LOGI(TAG, "Start ADC pipeline");
				audio_pipeline_run(adc_pipeline);
			} else if ((int)msg.data == BUTTON_ID_2) {
				ESP_LOGI(TAG, "[ * ] Button 2");
				ESP_LOGI(TAG, "Stop ADC pipeline");
				audio_pipeline_stop(adc_pipeline);
				audio_pipeline_wait_for_stop(adc_pipeline);
				audio_pipeline_reset_ringbuffer(adc_pipeline);
				audio_pipeline_reset_elements(adc_pipeline);
				audio_pipeline_terminate(adc_pipeline);
			} else if ((int)msg.data == BUTTON_ID_3) {
				ESP_LOGI(TAG, "[ * ] Button 3");
			}
		}
	}

	ESP_LOGI(TAG, "Stop ADC pipeline");
	audio_pipeline_stop(adc_pipeline);
	audio_pipeline_wait_for_stop(adc_pipeline);
	audio_pipeline_terminate(adc_pipeline);
	audio_pipeline_unregister(adc_pipeline, adc_i2s);

	/* Terminate the pipeline before removing the listener */
	audio_pipeline_remove_listener(adc_pipeline);

	/* Make sure audio_pipeline_remove_listener is called before destroying
	 * event_iface */
	audio_event_iface_destroy(evt);

	/* Release all resources */
	audio_pipeline_deinit(adc_pipeline);
	audio_element_deinit(adc_i2s);
	audio_element_deinit(dac_i2s);
}
