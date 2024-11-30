#pragma once

#include "http_stream.h"

esp_err_t _http_up_stream_event_handle(http_stream_event_msg_t *msg);
esp_err_t _http_down_stream_event_handle(http_stream_event_msg_t *msg);
