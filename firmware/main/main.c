#include "audio.h"
#include "epaper/DEV_Config.h"
#include "epaper/EPD_7in5_V2.h"
#include "epaper/caption.h"
#include "epaper/epaper.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "gattc.h"
#include "gatts.h"
#include "http_server.h"
#include "portmacro.h"
#include "wifi.h"

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

typedef enum {
	MODE_PAIR,
	MODE_LISTEN,
	MODE_TALK,
	MODE_SLEEP,
} badge_mode_t;

static badge_mode_t badge_mode = MODE_SLEEP;
static SemaphoreHandle_t fsm_sem;

void handle_button(int button_id) {
	if (xSemaphoreTake(fsm_sem, 0) == pdFALSE) {
		ESP_LOGW(TAG, "handle_button is busy, keypress ignored");
		return;
	}

	if (badge_mode == MODE_PAIR) {
		// TODO
		return;
	}

	if (badge_mode == MODE_SLEEP) {
		// TODO
		return;
	}

	// MODE_LISTEN or MODE_TALK
	if (button_id == BUTTON_ID_1) {
		if (badge_mode == MODE_LISTEN) {
			if (audio_pipeline_run(tx_pipeline) == ESP_OK) {
				ESP_LOGI(TAG, "MODE_TALK, Start ADC pipeline");
				badge_mode = MODE_TALK;
			}
			epaper_ui_set_layout(EPAPER_LAYOUT_CAPTION);
		} else {
			ESP_LOGI(TAG, "MODE_LISTEN, Stop ADC pipeline");
			badge_mode = MODE_LISTEN;

			audio_element_set_ringbuf_done(adc_i2s);
			audio_pipeline_stop(tx_pipeline);
			audio_pipeline_wait_for_stop(tx_pipeline);
			audio_pipeline_reset_ringbuffer(tx_pipeline);
			audio_pipeline_reset_elements(tx_pipeline);
			audio_pipeline_terminate(tx_pipeline);

			epaper_ui_set_layout(EPAPER_LAYOUT_BADGE);
		}
	} else if (button_id == BUTTON_ID_2) {
		ESP_LOGI(TAG, "MODE_PAIR, Start advertising name and IP address.");
		badge_mode = MODE_PAIR;
		gatts_init();

		ESP_LOGI(TAG, "Start Scanning for target device.");
		gattc_start();
		epaper_ui_set_layout(EPAPER_LAYOUT_PAIR);
	}

	xSemaphoreGive(fsm_sem);
}

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

	epaper_init();
	DEV_Delay_ms(500);
	epaper_ui_set_layout(EPAPER_LAYOUT_WIFI_CONNECTING);

	ESP_LOGI(TAG, "Start WiFi Connection!");
	wifi_init();

	epaper_ui_set_layout(EPAPER_LAYOUT_WIFI_CONNECTED);
	DEV_Delay_ms(1000);
	epaper_ui_set_layout(EPAPER_LAYOUT_BADGE);

	httpd_handle_t server = start_webserver();

	audio_init();

	bool tx_running = false;

	fsm_sem = xSemaphoreCreateBinary();
	xSemaphoreGive(fsm_sem);

	while (1) {
		audio_event_iface_msg_t msg;
		esp_err_t               ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
		if (ret != ESP_OK) {
			continue;
		}

		if (msg.source_type == PERIPH_ID_BUTTON && msg.cmd == PERIPH_BUTTON_PRESSED) {
			handle_button((int)msg.data);
			if ((int)msg.data == BUTTON_ID_1) {
			} else if ((int)msg.data == BUTTON_ID_2) {



			} else if ((int)msg.data == BUTTON_ID_3) {
				ESP_LOGI(TAG, "[ * ] Button 3");
			}
		}
	}

	audio_deinit();
}
