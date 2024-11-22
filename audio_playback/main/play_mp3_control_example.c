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
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "esp_wifi.h"
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

esp_err_t _http_stream_event_handle(http_stream_event_msg_t *msg) {
	esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
	char                     len_buf[16];
	static int               total_write = 0;

	if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
		// set header
		ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, length=%d",
		         msg->buffer_len);
		esp_http_client_set_method(http, HTTP_METHOD_POST);
		char dat[10] = {0};
		snprintf(dat, sizeof(dat), "%d", AUDIO_SAMPLE_RATE);
		esp_http_client_set_header(http, "x-audio-sample-rates", dat);
		memset(dat, 0, sizeof(dat));
		snprintf(dat, sizeof(dat), "%d", AUDIO_BITS);
		esp_http_client_set_header(http, "x-audio-bits", dat);
		memset(dat, 0, sizeof(dat));
		snprintf(dat, sizeof(dat), "%d", AUDIO_CHANNELS);
		esp_http_client_set_header(http, "x-audio-channel", dat);
		total_write = 0;
		return ESP_OK;
	}

	if (msg->event_id == HTTP_STREAM_ON_REQUEST) {
		// write data
		int wlen = sprintf(len_buf, "%x\r\n", msg->buffer_len);
		if (esp_http_client_write(http, len_buf, wlen) <= 0) {
			return ESP_FAIL;
		}
		if (esp_http_client_write(http, msg->buffer, msg->buffer_len) <= 0) {
			return ESP_FAIL;
		}
		if (esp_http_client_write(http, "\r\n", 2) <= 0) {
			return ESP_FAIL;
		}
		total_write += msg->buffer_len;
		printf("\033[A\33[2K\rTotal bytes written: %d\n", total_write);
		return msg->buffer_len;
	}

	if (msg->event_id == HTTP_STREAM_POST_REQUEST) {
		ESP_LOGI(TAG,
		         "[ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker");
		if (esp_http_client_write(http, "0\r\n\r\n", 5) <= 0) {
			return ESP_FAIL;
		}
		return ESP_OK;
	}

	if (msg->event_id == HTTP_STREAM_FINISH_REQUEST) {
		ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_FINISH_REQUEST");
		char *buf = calloc(1, 64);
		assert(buf);
		int read_len = esp_http_client_read(http, buf, 64);
		if (read_len <= 0) {
			free(buf);
			return ESP_FAIL;
		}
		buf[read_len] = 0;
		ESP_LOGI(TAG, "Got HTTP Response = %s", (char *)buf);
		free(buf);
		return ESP_OK;
	}
	return ESP_OK;
}

void app_main(void) {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}

	ESP_ERROR_CHECK(esp_netif_init());

	audio_pipeline_handle_t dac_pipeline, adc_pipeline;
	audio_element_handle_t  adc_i2s, dac_i2s, http_stream_writer;

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("wifi", ESP_LOG_ERROR);
	esp_log_level_set(TAG, ESP_LOG_INFO);

	ESP_LOGI(TAG, "Initialize peripherals");
	esp_periph_config_t     periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
	esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

	ESP_LOGI(TAG, "Initialize buttons & connect to Wi-Fi");
	audio_board_key_init(set);
	audio_board_wifi_init(set);

	ESP_LOGI(TAG, "Start audio codec chip");
	audio_board_handle_t board_handle = audio_board_init();
	audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH,
	                     AUDIO_HAL_CTRL_START);
	audio_hal_ctrl_codec(board_handle->adc_hal, AUDIO_HAL_CODEC_MODE_ENCODE,
			     AUDIO_HAL_CTRL_START);

	int player_volume, mic_volume;

	audio_hal_set_volume(board_handle->audio_hal, 100);
	audio_hal_get_volume(board_handle->audio_hal, &player_volume);
	ESP_LOGI(TAG, "DAC volume: %d", player_volume);

	audio_hal_set_volume(board_handle->adc_hal, 100);
	/* audio_hal_get_volume(board_handle->adc_hal, &mic_volume); */
	/* ESP_LOGI(TAG, "Vol: %d", mic_volume); */

	/* ADC */

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

	ESP_LOGI(TAG, "Create HTTP stream");
	http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
	http_cfg.type = AUDIO_STREAM_WRITER;
	http_cfg.event_handle = _http_stream_event_handle;
	http_stream_writer = http_stream_init(&http_cfg);
	audio_pipeline_register(adc_pipeline, http_stream_writer, "http");

	ESP_LOGI(TAG, "Link ADC pipeline");
	const char *link_tag[2] = {"adc", "http"};
	audio_pipeline_link(adc_pipeline, &link_tag[0], 2);

	ESP_LOGI(TAG, "Set up ADC event listener");
	audio_event_iface_cfg_t    adc_evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
	audio_event_iface_handle_t adc_evt = audio_event_iface_init(&adc_evt_cfg);

	ESP_LOGI(TAG, "Listening event from all elements of pipeline");
	audio_pipeline_set_listener(adc_pipeline, adc_evt);

	ESP_LOGI(TAG, "Listening event from peripherals");
	audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), adc_evt);

	/* DAC */

	ESP_LOGI(TAG, "Create DAC pipeline");
	audio_pipeline_cfg_t dac_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	dac_pipeline = audio_pipeline_init(&dac_pipeline_cfg);
	mem_assert(dac_pipeline);

	ESP_LOGI(TAG, "Create DAC i2s stream");
	i2s_stream_cfg_t dac_i2s_cfg = I2S_STREAM_CFG_DEFAULT();
	dac_i2s_cfg.type = AUDIO_STREAM_WRITER;
	dac_i2s = i2s_stream_init(&dac_i2s_cfg);
	i2s_stream_set_clk(dac_i2s, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
	audio_pipeline_register(dac_pipeline, dac_i2s, "dac");

	/* ESP_LOGI(TAG, "Start DAC pipeline"); */
	/* audio_pipeline_run(dac_pipeline); */

	while (1) {
		audio_event_iface_msg_t msg;
		esp_err_t ret = audio_event_iface_listen(adc_evt, &msg, portMAX_DELAY);
		if (ret != ESP_OK) {
			continue;
		}

		if (msg.source_type == PERIPH_ID_BUTTON && msg.cmd == PERIPH_BUTTON_PRESSED) {
			ESP_LOGI(TAG, "Button %d pressed", (int)msg.data);

			if ((int)msg.data == BUTTON_ID_1) {
				ESP_LOGI(TAG, "[ * ] Button 1");
				ESP_LOGI(TAG, "Start ADC pipeline");
				audio_element_set_uri(http_stream_writer, CONFIG_SERVER_URI);
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

	ESP_LOGI(TAG, "Stop DAC pipeline");
	/* audio_pipeline_stop(dac_pipeline);
	 * audio_pipeline_wait_for_stop(dac_pipeline);
	 * audio_pipeline_terminate(dac_pipeline);
	 * audio_pipeline_unregister(dac_pipeline, dac_i2s); */

	/* Terminate the pipeline before removing the listener */
	audio_pipeline_remove_listener(adc_pipeline);

	/* Make sure audio_pipeline_remove_listener is called before destroying
	 * event_iface */
	audio_event_iface_destroy(adc_evt);

	/* Release all resources */
	audio_pipeline_deinit(adc_pipeline);
	audio_pipeline_deinit(dac_pipeline);
	audio_element_deinit(adc_i2s);
	audio_element_deinit(http_stream_writer);
	audio_element_deinit(dac_i2s);
}
