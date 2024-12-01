#include "FreeRTOSConfig.h"
#include "epaper/caption.h"
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

static esp_err_t transcription_post_handler(httpd_req_t *req) {
	char buf[100];
	int  ret;
	int  bytes_recv = 0;

	while (bytes_recv < req->content_len) {
		/* Read the data for the request */
		ret = httpd_req_recv(req, buf + bytes_recv, req->content_len - bytes_recv);
		if (ret <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				/* Retry receiving if timeout occurred */
				continue;
			}
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
	return ESP_OK;
}

static const httpd_uri_t transcription_uri = {
    .uri = "/transcription",
    .method = HTTP_POST,
    .handler = transcription_post_handler,
    .user_ctx = NULL,
};

static esp_err_t audio_post_handler(httpd_req_t *req) {
	ESP_LOGI(TAG, "POST /audio");

	char len_buf[16];
	int  ret;
	int  bytes_recv = 0;

	// read to len_buf until we receive \r\n
	while (true) {
		ret = httpd_req_recv(req, len_buf + bytes_recv, 1);
		if (ret <= 0) {
			ESP_LOGW(TAG, "POST /audio failed to read length (ret = %d)", ret);
			return ESP_FAIL;
		}
		ESP_LOGI(TAG, "Received %c", len_buf[bytes_recv]);
		bytes_recv += ret;
		if (bytes_recv > 2 && len_buf[bytes_recv - 2] == '\r' &&
		    len_buf[bytes_recv - 1] == '\n') {
			break;
		}
	}

	size_t len = strtol(len_buf, NULL, 16); // payload length
	char *buf = calloc(len + 2, 1); // plus two bytes for \r\n
	ESP_LOGI(TAG, "POST /audio: chunk length is %lu", (long unsigned)len);

	ESP_LOGI(TAG, "============ POST /audio ===========");

	bytes_recv = 0;
	while (bytes_recv < len + 2) {
		/* Read the data for the request */
		ret = httpd_req_recv(req, buf + bytes_recv, len + 2 - bytes_recv);
		if (ret <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				/* Retry receiving if timeout occurred */
				continue;
			}
			return ESP_FAIL;
		}
		bytes_recv += ret;
		ESP_LOGI(TAG, "Read %lu bytes", (long unsigned)bytes_recv);
	}

	ESP_LOGI(TAG, "====================================");

	// End response
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

static const httpd_uri_t audio_uri = {
    .uri = "/audio",
    .method = HTTP_POST,
    .handler = audio_post_handler,
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
		httpd_register_uri_handler(server, &audio_uri);
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
