/* Play wav file by audio pipeline
   with possibility to start, stop, pause and resume playback
   as well as adjust volume

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "audio_hal.h"
#include "driver/i2c.h"
#include "driver/i2s_common.h"
#include "driver/i2s_types.h"
#include "freertos/task.h"
#include <string.h>

#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"
#include "board.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "i2s_stream.h"
#include "wav_decoder.h"
#include "periph_button.h"

static const char *TAG = "ECHO";

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_BITS 16
#define AUDIO_CHANNELS 1

void app_main(void) {
	audio_pipeline_handle_t dac_pipeline;
	audio_element_handle_t  adc_i2s, dac_i2s;

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set(TAG, ESP_LOG_INFO);

	ESP_LOGI(TAG, "[ 1 ] Start audio codec chip");
	audio_board_handle_t board_handle = audio_board_init();
	audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE,
	                     AUDIO_HAL_CTRL_START);
	audio_hal_ctrl_codec(board_handle->adc_hal, AUDIO_HAL_CODEC_MODE_ENCODE,
	                     AUDIO_HAL_CTRL_START);

	int player_volume, mic_volume;

	ESP_LOGI(TAG, "[ 2 ] Create audio pipeline, add all elements to "
	              "pipeline, and subscribe pipeline event");
	audio_pipeline_cfg_t dac_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	dac_pipeline = audio_pipeline_init(&dac_pipeline_cfg);
	mem_assert(dac_pipeline);

	ESP_LOGI(TAG, "[2.1] Create i2s stream to read data from ADC chip");
	i2s_stream_cfg_t adc_i2s_cfg = I2S_STREAM_CFG_DEFAULT();
	adc_i2s_cfg.type = AUDIO_STREAM_READER;
	adc_i2s = i2s_stream_init(&adc_i2s_cfg);
	i2s_stream_set_clk(adc_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);

	ESP_LOGI(TAG, "[2.2] Create i2s stream to write data to codec chip");
	i2s_stream_cfg_t dac_i2s_cfg = I2S_STREAM_CFG_DEFAULT();
	dac_i2s_cfg.type = AUDIO_STREAM_WRITER;
	dac_i2s = i2s_stream_init(&dac_i2s_cfg);
	i2s_stream_set_clk(dac_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);

	ESP_LOGI(TAG, "[2.3] Register all elements to audio pipeline");
	audio_pipeline_register(dac_pipeline, adc_i2s, "adc");
	audio_pipeline_register(dac_pipeline, dac_i2s, "dac");

	ESP_LOGI(TAG, "[2.4] Link it together");
	const char *link_tag[2] = {"adc", "dac"};
	audio_pipeline_link(dac_pipeline, &link_tag[0], 2);

	ESP_LOGI(TAG, "[ 3 ] Initialize peripherals");
	esp_periph_config_t     periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
	esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

	ESP_LOGI(TAG, "[3.1] Initialize keys on board");
	audio_board_key_init(set);

	ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
	audio_event_iface_cfg_t    evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
	audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

	ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
	audio_pipeline_set_listener(dac_pipeline, evt);

	ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
	audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

	audio_hal_set_volume(board_handle->audio_hal, 100);
	audio_hal_get_volume(board_handle->audio_hal, &player_volume);
	ESP_LOGI(TAG, "Vol: %d", player_volume);

	audio_hal_set_volume(board_handle->adc_hal, 100);
	/* audio_hal_get_volume(board_handle->adc_hal, &mic_volume); */
	/* ESP_LOGI(TAG, "Vol: %d", mic_volume); */

	ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
	audio_pipeline_run(dac_pipeline);

	while (1) {
		audio_event_iface_msg_t msg;
		esp_err_t               ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
		if (ret != ESP_OK) {
			continue;
		}

#define BUTTON_ID_1 42
#define BUTTON_ID_2 41
#define BUTTON_ID_3 40

		if (msg.source_type == PERIPH_ID_BUTTON && msg.cmd == PERIPH_BUTTON_PRESSED) {
			ESP_LOGI(TAG, "Button %d pressed", (int)msg.data);

			if ((int)msg.data == BUTTON_ID_1) {
				ESP_LOGI(TAG, "[ * ] [Play] event");
				audio_element_state_t el_state =
				    audio_element_get_state(dac_i2s);
				switch (el_state) {
				case AEL_STATE_INIT:
					ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
					audio_pipeline_run(dac_pipeline);
					break;
				case AEL_STATE_RUNNING:
					ESP_LOGI(TAG, "[ * ] Pausing audio pipeline");
					audio_pipeline_pause(dac_pipeline);
					break;
				case AEL_STATE_PAUSED:
					ESP_LOGI(TAG, "[ * ] Resuming audio pipeline");
					audio_pipeline_resume(dac_pipeline);
					break;
				case AEL_STATE_FINISHED:
					ESP_LOGI(TAG, "[ * ] Rewinding audio pipeline");
					audio_pipeline_reset_ringbuffer(dac_pipeline);
					audio_pipeline_reset_elements(dac_pipeline);
					audio_pipeline_change_state(dac_pipeline, AEL_STATE_INIT);
					audio_pipeline_run(dac_pipeline);
					break;
				default:
					ESP_LOGI(TAG, "[ * ] Not supported state %d", el_state);
				}
			} else if ((int)msg.data == BUTTON_ID_2) {
				ESP_LOGI(TAG, "[ * ] [Stop] event");
				ESP_LOGI(TAG, "[ * ] Stopping audio pipeline");
				break;
			} else if ((int)msg.data == BUTTON_ID_3) {
				ESP_LOGI(TAG, "[ * ] [Mode] event");
				audio_pipeline_stop(dac_pipeline);
				audio_pipeline_wait_for_stop(dac_pipeline);
				audio_pipeline_terminate(dac_pipeline);
				audio_pipeline_reset_ringbuffer(dac_pipeline);
				audio_pipeline_reset_elements(dac_pipeline);
			        audio_pipeline_run(dac_pipeline);
			}
		}
	}

	ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
	audio_pipeline_stop(dac_pipeline);
	audio_pipeline_wait_for_stop(dac_pipeline);
	audio_pipeline_terminate(dac_pipeline);
	audio_pipeline_unregister(dac_pipeline, adc_i2s);
	audio_pipeline_unregister(dac_pipeline, dac_i2s);

	/* Terminate the pipeline before removing the listener */
	audio_pipeline_remove_listener(dac_pipeline);

	/* Make sure audio_pipeline_remove_listener is called before destroying
	 * event_iface */
	audio_event_iface_destroy(evt);

	/* Release all resources */
	audio_pipeline_deinit(dac_pipeline);
	audio_element_deinit(adc_i2s);
	audio_element_deinit(dac_i2s);
}
