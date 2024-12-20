#include "audio.h"
#include "es8311.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "http_stream.h"
#include "sdkconfig.h"
#include <stdio.h>

static const char *TAG = "http_client";

esp_err_t _http_up_stream_event_handle(http_stream_event_msg_t *msg) {
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
		ESP_LOGI(TAG, "Total bytes written: %d", total_write);
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

esp_err_t _http_down_stream_event_handle(http_stream_event_msg_t *msg) {
	return ESP_OK;
}

esp_err_t http_request_to_pair(const char *peer_ip) {
	char query[30];
	snprintf(query, 30, "with=%s", peer_ip);
	esp_http_client_config_t config = {
	    .host = CONFIG_SERVER_IP,
	    .port = atoi(CONFIG_SERVER_PORT),
	    .path = "/pair",
	    .query = query,
	    .event_handler = NULL,
	    .user_data = NULL,
	    .timeout_ms = 10000,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_err_t err = esp_http_client_perform(client);
	// blocks task until response
	if (err == ESP_OK) {
		int status = esp_http_client_get_status_code(client);
		ESP_LOGI(TAG, "HTTP GET /pair?with=%s Status = %d", peer_ip, status);
		if (status == 200) {
			ESP_LOGI(TAG, "Server accepted pair request with %s", peer_ip);
		} else {
			ESP_LOGW(TAG, "Server rejected pair request with %s", peer_ip);
			return ESP_FAIL;
		}
	} else {
		ESP_LOGE(TAG, "HTTP GET /pair?with=%s failed: %s", peer_ip, esp_err_to_name(err));
		return ESP_FAIL;
	}
	return ESP_OK;
}

esp_err_t http_request_to_unpair() {
	esp_http_client_config_t config = {
	    .host = CONFIG_SERVER_IP,
	    .port = atoi(CONFIG_SERVER_PORT),
	    .path = "/unpair",
	    .event_handler = NULL,
	    .user_data = NULL,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_err_t err = esp_http_client_perform(client);
	// blocks task until response
	if (err == ESP_OK) {
		int status = esp_http_client_get_status_code(client);
		ESP_LOGI(TAG, "HTTP GET /unpair Status = %d", status);
		if (status == 200) {
			ESP_LOGI(TAG, "Server accepted unpair request");
		} else {
			ESP_LOGW(TAG, "Server rejected unpair request");
			return ESP_FAIL;
		}
	} else {
		ESP_LOGE(TAG, "HTTP GET /unpair failed: %s", esp_err_to_name(err));
		return ESP_FAIL;
	}
	return ESP_OK;
}
