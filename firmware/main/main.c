#include "audio.h"
#include "epaper/DEV_Config.h"
#include "epaper/epaper.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "gattc.h"
#include "gatts.h"
#include "http_server.h"
#include "portmacro.h"
#include "ui.h"
#include "wifi.h"

#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"
#include "board.h"
#include "es8311.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_peripherals.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "periph_button.h"

static const char *TAG = "main";

typedef enum {
	MODE_LISTEN,
	MODE_TALK,
	MODE_SLEEP,
	MODE_PAIR_SEARCH,
	MODE_PAIR_CONFIRM,
	MODE_PAIR_NO_PEER,
	MODE_PAIR_PENDING,
	MODE_PAIR_RESULT,
} badge_mode_t;

static badge_mode_t      badge_mode = MODE_SLEEP;
user_t                   peer_badge;

void handle_button(int button_id) {
	if (badge_mode == MODE_LISTEN) {
		if (button_id == BUTTON_ID_1) {
			if (audio_pipeline_run(tx_pipeline) == ESP_OK) {
				ESP_LOGI(TAG, "MODE_TALK, Start ADC pipeline");
				badge_mode = MODE_TALK;
				ui_layout_caption();
			}
		} else if (button_id == BUTTON_ID_2) {
			ESP_LOGI(TAG, "MODE_PAIR_SEARCH, Start Scanning for target device");
			badge_mode = MODE_PAIR_SEARCH;
			gattc_start();
			ui_layout_pair_searching();
			// begin searching for peer
			if (xQueueReceive(ble_device_queue, &peer_badge, pdMS_TO_TICKS(5000)) ==
			    pdTRUE) {
				// found peer
				ESP_LOGI(TAG, "MODE_PAIR_CONFIRM");
				badge_mode = MODE_PAIR_CONFIRM;
				ESP_LOGI(TAG, "BLE peer name: %s", peer_badge.name);
				ESP_LOGI(TAG, "BLE peer IP: %s", peer_badge.ip);
				ui_layout_pair_confirm(peer_badge.name);
			} else {
				ESP_LOGI(TAG, "MODE_PAIR_NO_PEER");
				badge_mode = MODE_PAIR_NO_PEER;
				ESP_LOGW(TAG, "No badges scanned over BLE");
				ui_layout_pair_confirm(NULL); // display failure message
			}
		}
	} else if (badge_mode == MODE_TALK) {
		if (button_id == BUTTON_ID_1) {
			ESP_LOGI(TAG, "MODE_LISTEN, Stop ADC pipeline");
			badge_mode = MODE_LISTEN;

			audio_element_set_ringbuf_done(adc_i2s);
			audio_pipeline_stop(tx_pipeline);
			audio_pipeline_wait_for_stop(tx_pipeline);
			audio_pipeline_reset_ringbuffer(tx_pipeline);
			audio_pipeline_reset_elements(tx_pipeline);
			audio_pipeline_terminate(tx_pipeline);

			ui_layout_badge();
		}
	} else if (badge_mode == MODE_PAIR_SEARCH) {
		// TODO
		badge_mode = MODE_LISTEN;
		ui_layout_badge();
	} else if (badge_mode == MODE_PAIR_NO_PEER) {
		// press any key to go to listen mode
		ESP_LOGI(TAG, "MODE_LISTEN");
		badge_mode = MODE_LISTEN;
		ui_layout_badge();
	} else if (badge_mode == MODE_PAIR_CONFIRM) {
		if (button_id == BUTTON_ID_1) {
			// confirm
			ESP_LOGI(TAG, "MODE_PAIR_PENDING");
			badge_mode = MODE_PAIR_PENDING;
		} else if (button_id == BUTTON_ID_2) {
			// cancel
			ESP_LOGI(TAG, "MODE_PAIR_PENDING");
			badge_mode = MODE_PAIR_PENDING;
			ui_layout_badge();
		}
	} else if (badge_mode == MODE_SLEEP) {
		// TODO
	}
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
	ui_layout_wifi_connecting();

	ESP_LOGI(TAG, "Start WiFi Connection and advertise on BLE");
	wifi_init();
	gatts_init();

	ui_layout_wifi_connected();
	DEV_Delay_ms(1000);
	ui_layout_badge();

	httpd_handle_t server = start_webserver();

	audio_init();
	es8311_pa_power(false);

	badge_mode = MODE_LISTEN;

	while (1) {
		audio_event_iface_msg_t msg;
		esp_err_t               ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
		if (ret != ESP_OK) {
			continue;
		}

		if (msg.source_type == PERIPH_ID_BUTTON && msg.cmd == PERIPH_BUTTON_PRESSED) {
			handle_button((int)msg.data);
		}
	}

	audio_deinit();
}
