/* Record WAV file to SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_http_client.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "board.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "periph_wifi.h"
#include "filter_resample.h"
#include "input_key_service.h"
#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "REC_RAW_HTTP";
char* currentText;

#define EXAMPLE_AUDIO_SAMPLE_RATE  (16000)
#define EXAMPLE_AUDIO_BITS         (16)
#define EXAMPLE_AUDIO_CHANNELS     (1)

#define DEMO_EXIT_BIT (BIT0)

#define REQUEST_INTERVAL_MS 1000
#define PRINT_TEXT_INTERVAL_MS 1000
#define BUFFER_SIZE 50
#define MAX_HTTP_RECV_BUFFER 2048

static EventGroupHandle_t EXIT_FLAG;

audio_pipeline_handle_t pipeline;
audio_pipeline_handle_t pipeline2;
audio_element_handle_t i2s_stream_reader;
audio_element_handle_t http_stream_writer;
audio_element_handle_t http_stream_writer2;

void print_current_text(void *pvParameters){
    while(1){
        ESP_LOGI(TAG, "current text stored in buffer: %s", currentText);
        vTaskDelay(pdMS_TO_TICKS(PRINT_TEXT_INTERVAL_MS));  // Delay for the specified interval
    }
}


void http_get_task(void *pvParameters)
{
    while(1){
        char *buffer = malloc(MAX_HTTP_RECV_BUFFER + 1);
        if (buffer == NULL) {
            ESP_LOGE(TAG, "Cannot malloc http receive buffer");
            return;
        }
        esp_http_client_config_t config = {
            .url = "http://192.168.0.193:8000",
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t errGet;
        if ((errGet = esp_http_client_open(client, 0)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(errGet));
            free(buffer);
            return;
        }
        int content_length =  esp_http_client_fetch_headers(client);
        int total_read_len = 0, read_len;
        if (total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER) {
            read_len = esp_http_client_read(client, buffer, content_length);
            if (read_len <= 0) {
                ESP_LOGE(TAG, "Error read data");
            }
            buffer[read_len] = 0;
            ESP_LOGI(TAG, "read_len = %d", read_len);
        }
        ESP_LOGI(TAG, "buffer: %s", buffer);
        memcpy(currentText, buffer, strlen(buffer));
        currentText[strlen(buffer)] = 0;
        ESP_LOGI(TAG, "HTTP Stream reader Status = %d, content_length = %"PRId64,
                        esp_http_client_get_status_code(client),
                        esp_http_client_get_content_length(client));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(buffer);
        vTaskDelay(pdMS_TO_TICKS(REQUEST_INTERVAL_MS));  // Delay for the specified interval
    }
}

esp_err_t _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
    char len_buf[16];
    static int total_write = 0;

    if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
        // set header
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, lenght=%d", msg->buffer_len);
        esp_http_client_set_method(http, HTTP_METHOD_POST);
        char dat[10] = {0};
        snprintf(dat, sizeof(dat), "%d", EXAMPLE_AUDIO_SAMPLE_RATE);
        esp_http_client_set_header(http, "x-audio-sample-rates", dat);
        memset(dat, 0, sizeof(dat));
        snprintf(dat, sizeof(dat), "%d", EXAMPLE_AUDIO_BITS);
        esp_http_client_set_header(http, "x-audio-bits", dat);
        memset(dat, 0, sizeof(dat));
        snprintf(dat, sizeof(dat), "%d", EXAMPLE_AUDIO_CHANNELS);
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
        //printf("\033[A\33[2K\rTotal bytes written: %d\n", total_write);
        return msg->buffer_len;
    }

    if (msg->event_id == HTTP_STREAM_POST_REQUEST) {
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker");
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

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    audio_element_handle_t http_stream_writer = (audio_element_handle_t)ctx;
    //audio_element_handle_t http_stream_writer2 = (audio_element_handle_t)ctx;
    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK) {
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_MODE:
                ESP_LOGW(TAG, "[ * ] [Set] input key event, exit the demo ...");
                xEventGroupSetBits(EXIT_FLAG, DEMO_EXIT_BIT);
                break;
            case INPUT_KEY_USER_ID_REC:
                ESP_LOGE(TAG, "[ * ] [Rec] input key event, resuming pipeline ...");
                /*
                 * There is no effect when follow APIs output warning message on the first time record
                 */
                audio_pipeline_stop(pipeline);
                audio_pipeline_stop(pipeline2);

                audio_pipeline_wait_for_stop(pipeline);
                audio_pipeline_wait_for_stop(pipeline2);

                audio_pipeline_reset_ringbuffer(pipeline);
                audio_pipeline_reset_ringbuffer(pipeline2);

                audio_pipeline_reset_elements(pipeline);
                audio_pipeline_reset_elements(pipeline2);

                audio_pipeline_terminate(pipeline);
                audio_pipeline_terminate(pipeline2);

                audio_element_set_uri(http_stream_writer, CONFIG_SERVER_URI);
                audio_element_set_uri(http_stream_writer2, "http://192.168.0.200:8000/upload");

                audio_pipeline_run(pipeline);
                audio_pipeline_run(pipeline2);
                break;
        }
    } else if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE || evt->type == INPUT_KEY_SERVICE_ACTION_PRESS_RELEASE) {
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_REC:
                ESP_LOGE(TAG, "[ * ] [Rec] key released, stop pipeline ...");
                /*
                 * Set the i2s_stream_reader ringbuffer is done to flush the buffering voice data.
                 */
                audio_element_set_ringbuf_done(i2s_stream_reader);
                break;
        }
    }

    return ESP_OK;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    EXIT_FLAG = xEventGroupCreate();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    ESP_LOGI(TAG, "[ 1 ] Initialize Button Peripheral & Connect to wifi network");
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    periph_wifi_cfg_t wifi_cfg = {
        .wifi_config.sta.ssid = CONFIG_WIFI_SSID,
        .wifi_config.sta.password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);

    // Start wifi & button peripheral
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[3.1] Create http stream to post data to server");

    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.type = AUDIO_STREAM_WRITER;
    http_cfg.event_handle = _http_stream_event_handle;
    http_stream_writer = http_stream_init(&http_cfg);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(CODEC_ADC_I2S_PORT, 44100, 16, AUDIO_STREAM_READER, 1);
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.out_rb_size = 16 * 1024; // Increase buffer to avoid missing data in bad network conditions
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, http_stream_writer, "http");

    ESP_LOGI(TAG, "[3.4] Link it together [codec_chip]-->i2s_stream->http_stream-->[http_server]");
    const char *link_tag2[2] = {"i2s", "http"};
    audio_pipeline_link(pipeline, &link_tag2[0], 2);

    ESP_LOGI(TAG, "[4.0] Create the SECOND pipeline for transmitting audio to another badge");
    audio_pipeline_cfg_t pipeline_cfg2 = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline2 = audio_pipeline_init(&pipeline_cfg2);
    mem_assert(pipeline2);

    ESP_LOGI(TAG, "[4.1] Create ANOTHER http stream to post data to server");

    http_stream_cfg_t http_cfg2 = HTTP_STREAM_CFG_DEFAULT();
    http_cfg2.task_core = 0;
    http_cfg2.type = AUDIO_STREAM_WRITER;
    http_cfg2.event_handle = _http_stream_event_handle;
    http_stream_writer2 = http_stream_init(&http_cfg2);

    ESP_LOGI(TAG, "[4.2] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline2, http_stream_writer2, "http");

    ESP_LOGI(TAG, "[4.3] Link it together http_stream-->[http_server]");
    const char *link_tag[1] = {"http"};
    audio_pipeline_link(pipeline2, &link_tag[0], 1);

    ESP_LOGI(TAG, "[4.4] Connect input ringbuffer of pipeline_save to http stream multi output");
    ringbuf_handle_t rb = audio_element_get_output_ringbuf(http_stream_writer2);
    audio_element_set_multi_output_ringbuf(i2s_stream_reader, rb, 0);

    // Initialize Button peripheral
    audio_board_key_init(set);
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, (void *)http_stream_writer);

    i2s_stream_set_clk(i2s_stream_reader, EXAMPLE_AUDIO_SAMPLE_RATE, EXAMPLE_AUDIO_BITS, EXAMPLE_AUDIO_CHANNELS);
    
    ESP_LOGI(TAG, "[5] Setup the task to send HTTP GET Request periodically");
    currentText = (char*)calloc(20, 1);
    xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 1, NULL);
    xTaskCreate(&print_current_text, "print_current_text", 4096, NULL, 1, NULL);

    ESP_LOGI(TAG, "[ 6 ] Press [Rec] button to record, Press [Mode] to exit");
    xEventGroupWaitBits(EXIT_FLAG, DEMO_EXIT_BIT, true, false, portMAX_DELAY); // wait for interrupt for reset or exit.

    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);

    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, http_stream_writer);
    audio_pipeline_unregister(pipeline, i2s_stream_reader);

    /* Terminal the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_pipeline_deinit(pipeline2);
    audio_element_deinit(http_stream_writer);
    audio_element_deinit(http_stream_writer2);
    audio_element_deinit(i2s_stream_reader);
    esp_periph_set_destroy(set);
}
