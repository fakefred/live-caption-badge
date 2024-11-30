#include "audio.h"
#include "epaper/DEV_Config.h"
#include "epaper/caption.h"
#include "epaper/epaper.h"
#include "http_server.h"

#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"
#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_peripherals.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "periph_button.h"

static const char *TAG = "main";

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
	esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
	periph_set = esp_periph_set_init(&periph_cfg);

	audio_board_key_init(periph_set);
	audio_board_wifi_init(periph_set);

	audio_init();

	epaper_init();
	DEV_Delay_ms(500);
	epaper_ui_set_layout(EPAPER_LAYOUT_BADGE);

	httpd_handle_t server = start_webserver();

	/* ESP_LOGI(TAG, "Start DAC pipeline");
	 * audio_pipeline_run(dac_pipeline); */

	while (1) {
		audio_event_iface_msg_t msg;
		esp_err_t               ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
		if (ret != ESP_OK) {
			continue;
		}

		if (msg.source_type == PERIPH_ID_BUTTON && msg.cmd == PERIPH_BUTTON_PRESSED) {
			if ((int)msg.data == BUTTON_ID_1) {
				ESP_LOGI(TAG, "[ * ] Button 1");
				ESP_LOGI(TAG, "Start ADC pipeline");
				audio_element_set_uri(http_up_stream, CONFIG_SERVER_URI);
				audio_pipeline_run(adc_pipeline);
				epaper_ui_set_layout(EPAPER_LAYOUT_CAPTION);
			} else if ((int)msg.data == BUTTON_ID_2) {
				ESP_LOGI(TAG, "[ * ] Button 2");
				ESP_LOGI(TAG, "Stop ADC pipeline");
				audio_element_set_ringbuf_done(adc_i2s);
				audio_pipeline_stop(adc_pipeline);
				audio_pipeline_wait_for_stop(adc_pipeline);
				audio_pipeline_reset_ringbuffer(adc_pipeline);
				audio_pipeline_reset_elements(adc_pipeline);
				audio_pipeline_terminate(adc_pipeline);
				epaper_ui_set_layout(EPAPER_LAYOUT_BADGE);
			} else if ((int)msg.data == BUTTON_ID_3) {
				ESP_LOGI(TAG, "[ * ] Button 3");
			}
		}
	}

	audio_deinit();
}