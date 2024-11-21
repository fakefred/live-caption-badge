/* Play wav file by audio pipeline
   with possibility to start, stop, pause and resume playback
   as well as adjust volume

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "audio_hal.h"
#include "driver/gpio.h"
#include "es8311.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_pipeline.h"
#include "board.h"
#include "board_def.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "i2s_stream.h"
#include "wav_decoder.h"
#include "periph_adc_button.h"
#include "periph_button.h"
#include "periph_touch.h"

static const char *TAG = "PLAY_FLASH_MP3_CONTROL";

static struct marker {
	int            pos;
	const uint8_t *start;
	const uint8_t *end;
} file_marker;

extern const uint8_t mr_wav_start[] asm("_binary_audio_wav_start");
extern const uint8_t mr_wav_end[] asm("_binary_audio_wav_end");

static void set_next_file_marker() {
	file_marker.start = mr_wav_start;
	file_marker.end = mr_wav_end;
	file_marker.pos = 0;
}

int wav_music_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time,
                      void *ctx) {
	int read_size = file_marker.end - file_marker.start - file_marker.pos;
	if (read_size == 0) {
		return AEL_IO_DONE;
	} else if (len < read_size) {
		read_size = len;
	}
	memcpy(buf, file_marker.start + file_marker.pos, read_size);
	file_marker.pos += read_size;
	return read_size;
}

void app_main(void) {
	audio_pipeline_handle_t pipeline;
	audio_element_handle_t  i2s_stream_writer, wav_decoder;

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set(TAG, ESP_LOG_INFO);

	ESP_LOGI(TAG, "[ 1 ] Start audio codec chip");
	audio_board_handle_t board_handle = audio_board_init();
	audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE,
	                     AUDIO_HAL_CTRL_START);

	int player_volume;
	audio_hal_get_volume(board_handle->audio_hal, &player_volume);

	ESP_LOGI(TAG, "[ 2 ] Create audio pipeline, add all elements to "
	              "pipeline, and subscribe pipeline event");
	audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	pipeline = audio_pipeline_init(&pipeline_cfg);
	mem_assert(pipeline);

	ESP_LOGI(TAG, "[2.1] Create wav decoder to decode wav file and set "
	              "custom read callback");
	wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
	wav_decoder = wav_decoder_init(&wav_cfg);
	audio_element_set_read_cb(wav_decoder, wav_music_read_cb, NULL);

	ESP_LOGI(TAG, "[2.2] Create i2s stream to write data to codec chip");
#if defined CONFIG_ESP32_C3_LYRA_V2_BOARD
	i2s_stream_cfg_t i2s_cfg = I2S_STREAM_PDM_TX_CFG_DEFAULT();
#else
	i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
#endif
	i2s_cfg.type = AUDIO_STREAM_WRITER;
	i2s_stream_writer = i2s_stream_init(&i2s_cfg);

	ESP_LOGI(TAG, "[2.3] Register all elements to audio pipeline");
	audio_pipeline_register(pipeline, wav_decoder, "wav");
	audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

	ESP_LOGI(TAG, "[2.4] Link it together "
	              "[wav_music_read_cb]-->wav_decoder-->i2s_stream-->[codec_chip]");
	const char *link_tag[2] = {"wav", "i2s"};
	audio_pipeline_link(pipeline, &link_tag[0], 2);

	ESP_LOGI(TAG, "[ 3 ] Initialize peripherals");
	esp_periph_config_t     periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
	esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

	ESP_LOGI(TAG, "[3.1] Initialize keys on board");
	audio_board_key_init(set);

	ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
	audio_event_iface_cfg_t    evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
	audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

	ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
	audio_pipeline_set_listener(pipeline, evt);

	ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
	audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

	ESP_LOGW(TAG, "[ 5 ] Tap touch buttons to control music player:");
	ESP_LOGW(TAG, "      [Play] to start, pause and resume, [Set] to stop.");
	ESP_LOGW(TAG, "      [Vol-] or [Vol+] to adjust volume.");

	ESP_LOGI(TAG, "[ 5.1 ] Start audio_pipeline");
	set_next_file_marker();
	audio_pipeline_run(pipeline);

	audio_hal_set_volume(board_handle->audio_hal, 90);
	audio_hal_get_volume(board_handle->audio_hal, &player_volume);
	ESP_LOGI(TAG, "Vol: %d", player_volume);

	while (1) {
		audio_event_iface_msg_t msg;
		esp_err_t               ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
		if (ret != ESP_OK) {
			continue;
		}

		if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
		    msg.source == (void *)wav_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
			audio_element_info_t music_info = {0};
			audio_element_getinfo(wav_decoder, &music_info);
			ESP_LOGI(TAG,
			         "[ * ] Receive music info from wav decoder, "
			         "sample_rates=%d, bits=%d, ch=%d",
			         music_info.sample_rates, music_info.bits, music_info.channels);
			i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates,
			                   music_info.bits, music_info.channels);
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
				    audio_element_get_state(i2s_stream_writer);
				switch (el_state) {
				case AEL_STATE_INIT:
					ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
					audio_pipeline_run(pipeline);
					break;
				case AEL_STATE_RUNNING:
					ESP_LOGI(TAG, "[ * ] Pausing audio pipeline");
					audio_pipeline_pause(pipeline);
					break;
				case AEL_STATE_PAUSED:
					ESP_LOGI(TAG, "[ * ] Resuming audio pipeline");
					audio_pipeline_resume(pipeline);
					break;
				case AEL_STATE_FINISHED:
					ESP_LOGI(TAG, "[ * ] Rewinding audio pipeline");
					audio_pipeline_reset_ringbuffer(pipeline);
					audio_pipeline_reset_elements(pipeline);
					audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
					set_next_file_marker();
					audio_pipeline_run(pipeline);
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
				audio_pipeline_stop(pipeline);
				audio_pipeline_wait_for_stop(pipeline);
				audio_pipeline_terminate(pipeline);
				audio_pipeline_reset_ringbuffer(pipeline);
				audio_pipeline_reset_elements(pipeline);
				set_next_file_marker();
			        audio_pipeline_run(pipeline);
			}
		}
	}

	ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
	audio_pipeline_stop(pipeline);
	audio_pipeline_wait_for_stop(pipeline);
	audio_pipeline_terminate(pipeline);
	audio_pipeline_unregister(pipeline, wav_decoder);
	audio_pipeline_unregister(pipeline, i2s_stream_writer);

	/* Terminate the pipeline before removing the listener */
	audio_pipeline_remove_listener(pipeline);

	/* Make sure audio_pipeline_remove_listener is called before destroying
	 * event_iface */
	audio_event_iface_destroy(evt);

	/* Release all resources */
	audio_pipeline_deinit(pipeline);
	audio_element_deinit(i2s_stream_writer);
	audio_element_deinit(wav_decoder);
}
