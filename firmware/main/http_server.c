#include "DEV_Config.h"
#include "audio.h"
#include "gattc.h"
#include "gatts.h"
#include "ui.h"

#include "FreeRTOSConfig.h"
#include "epaper/caption.h"
#include "es8311.h"
#include "esp_err.h"
#include "ringbuf.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

static const char *TAG = "http_server";

extern char name[20];
extern char pronouns[20];
extern char affiliation[30];
extern char role[20];

extern bool   paired;
extern user_t peer_badge;

static esp_err_t transcription_post_handler(httpd_req_t *req) {
	char *buf = calloc(req->content_len + 1, 1);
	int   ret;
	int   bytes_recv = 0;

	while (bytes_recv < req->content_len) {
		/* Read the data for the request */
		ret = httpd_req_recv(req, buf + bytes_recv, req->content_len - bytes_recv);
		if (ret <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				/* Retry receiving if timeout occurred */
				continue;
			}
			free(buf);
			return ESP_FAIL;
		}

		bytes_recv += ret;
	}

	/* Log data received */
	ESP_LOGI(TAG, "======== POST /transcription =======");
	ESP_LOGI(TAG, "%.*s", bytes_recv, buf);
	ESP_LOGI(TAG, "====================================");

	buf[bytes_recv] = '\0';

	caption_append(buf);

	// End response
	httpd_resp_send_chunk(req, NULL, 0);
	free(buf);
	return ESP_OK;
}

static const httpd_uri_t transcription_uri = {
    .uri = "/transcription",
    .method = HTTP_POST,
    .handler = transcription_post_handler,
    .user_ctx = NULL,
};

static esp_err_t reboot_get_handler(httpd_req_t *req) {
	ESP_LOGI(TAG, "======== GET /reboot =======");

	// End response
	httpd_resp_send_chunk(req, NULL, 0);

	esp_restart();
	return ESP_OK;
}

static const httpd_uri_t reboot_uri = {
    .uri = "/reboot",
    .method = HTTP_GET,
    .handler = reboot_get_handler,
    .user_ctx = NULL,
};

static esp_err_t user_post_handler(httpd_req_t *req) {
	char *buf = calloc(req->content_len + 1, 1);
	int   ret;
	int   bytes_recv = 0;

	while (bytes_recv < req->content_len) {
		/* Read the data for the request */
		ret = httpd_req_recv(req, buf + bytes_recv, req->content_len - bytes_recv);
		if (ret <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				/* Retry receiving if timeout occurred */
				continue;
			}
			free(buf);
			return ESP_FAIL;
		}

		bytes_recv += ret;
	}

	/* Log data received */
	ESP_LOGI(TAG, "======== POST /user =======");
	ESP_LOGI(TAG, "%.*s", bytes_recv, buf);
	ESP_LOGI(TAG, "===========================");

	buf[bytes_recv] = '\0';

	char *tok = strtok(buf, ";");
	if (tok == NULL) {
		return ESP_FAIL;
	}
	strncpy(name, tok, sizeof(name));
	name[sizeof(name) - 1] = '\0';

	tok = strtok(NULL, ";");
	if (tok == NULL) {
		return ESP_FAIL;
	}
	strncpy(pronouns, tok, sizeof(pronouns));
	pronouns[sizeof(pronouns) - 1] = '\0';

	tok = strtok(NULL, ";");
	if (tok == NULL) {
		return ESP_FAIL;
	}
	strncpy(affiliation, tok, sizeof(affiliation));
	affiliation[sizeof(affiliation) - 1] = '\0';

	tok = strtok(NULL, ";");
	if (tok == NULL) {
		return ESP_FAIL;
	}
	strncpy(role, tok, sizeof(role));
	role[sizeof(role) - 1] = '\0';

	ui_layout_badge(paired ? peer_badge.name : NULL);
	gatts_deinit();
	DEV_Delay_ms(500);
	gatts_init();

	// End response
	httpd_resp_send(req, NULL, 0);
	free(buf);
	return ESP_OK;
}

static const httpd_uri_t user_uri = {
    .uri = "/user",
    .method = HTTP_POST,
    .handler = user_post_handler,
    .user_ctx = NULL,
};

static esp_err_t poke_get_handler(httpd_req_t *req) {
	ESP_LOGI(TAG, "======== GET /poke =======");

	if (audio_pipeline_run(rx_pipeline) == ESP_OK) {
		es8311_pa_power(true);
	} else {
		ESP_LOGE(TAG, "Failed to run rx_pipeline");
	}

	// End response
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

static const httpd_uri_t poke_uri = {
    .uri = "/poke",
    .method = HTTP_GET,
    .handler = poke_get_handler,
    .user_ctx = NULL,
};

static esp_err_t unpoke_get_handler(httpd_req_t *req) {
	ESP_LOGI(TAG, "======== GET /unpoke =======");

	if (audio_pipeline_stop(rx_pipeline) == ESP_OK) {
		ESP_LOGI(TAG, "No more audio from server");
		audio_pipeline_stop(rx_pipeline);
		audio_pipeline_wait_for_stop(rx_pipeline);
		audio_pipeline_reset_ringbuffer(rx_pipeline);
		audio_pipeline_reset_elements(rx_pipeline);
		audio_pipeline_terminate(rx_pipeline);
		es8311_pa_power(false);
	} else {
		ESP_LOGE(TAG, "Failed to stop rx_pipeline");
	}

	// End response
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

static const httpd_uri_t unpoke_uri = {
    .uri = "/unpoke",
    .method = HTTP_GET,
    .handler = unpoke_get_handler,
    .user_ctx = NULL,
};

httpd_handle_t start_webserver(void) {
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.lru_purge_enable = true;

	// Start the httpd server
	ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) == ESP_OK) {
		// Set URI handlers
		ESP_LOGI(TAG, "Registering URI handlers");
		httpd_register_uri_handler(server, &transcription_uri);
		httpd_register_uri_handler(server, &reboot_uri);
		httpd_register_uri_handler(server, &user_uri);
		httpd_register_uri_handler(server, &poke_uri);
		httpd_register_uri_handler(server, &unpoke_uri);
		return server;
	}

	ESP_LOGE(TAG, "Error starting server!");
	return NULL;
}

esp_err_t stop_webserver(httpd_handle_t server) {
	// Stop the httpd server
	return httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {
	httpd_handle_t *server = (httpd_handle_t *)arg;
	if (*server) {
		ESP_LOGI(TAG, "Stopping webserver");
		if (stop_webserver(*server) == ESP_OK) {
			*server = NULL;
		} else {
			ESP_LOGE(TAG, "Failed to stop http server");
		}
	}
}

static void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                            void *event_data) {
	httpd_handle_t *server = (httpd_handle_t *)arg;
	if (*server == NULL) {
		ESP_LOGI(TAG, "Starting webserver");
		*server = start_webserver();
	}
}
