#pragma once

#include "http_stream.h"

esp_err_t _http_up_stream_event_handle(http_stream_event_msg_t *msg);
esp_err_t _http_down_stream_event_handle(http_stream_event_msg_t *msg);

esp_err_t http_request_to_pair(const char *peer_ip);
esp_err_t http_request_to_unpair();
