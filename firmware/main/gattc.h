#ifndef GATTC_H
#define GATTC_H

#include "nvs.h"
#include "nvs_flash.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define GATTC_TAG "GATTC_DEMO"
#define REMOTE_SERVICE_UUID 0x00FF
#define REMOTE_NOTIFY_CHAR_UUID 0xFF01
#define REMOTE_READ_CHAR_UUID 0xFF02
#define REMOTE_READ_CHAR_UUID2 0xFF03
#define PROFILE_NUM 2
#define PROFILE_A_APP_ID 1
#define INVALID_HANDLE 0

#define MAX_DEVICES 1
#define DEVICE_NAME_FILTER "LIVE"

// static const char remote_device_name[] = "LIVE_CAPTION_BADGE";
static bool                    connect = false;
static bool                    get_server = false;
static esp_gattc_char_elem_t  *char_elem_result = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;

extern QueueHandle_t ble_device_queue;

// for multiple device connection
typedef struct {
	esp_bd_addr_t       bda;
	esp_ble_addr_type_t addr_type;
} ble_device_t;

typedef struct {
	char name[20];
	char ip[20];
} user_t;

void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                  esp_ble_gattc_cb_param_t *param);
void gattc_profile_event_handler(esp_gattc_cb_event_t      event,
                                 esp_gatt_if_t             gattc_if,
                                 esp_ble_gattc_cb_param_t *param);
esp_err_t gattc_start(void);

static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid =
        {
            .uuid16 = REMOTE_SERVICE_UUID,
        },
};

static esp_bt_uuid_t remote_filter_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid =
        {
            .uuid16 = REMOTE_READ_CHAR_UUID,
        },
};

static esp_bt_uuid_t remote_filter_char_uuid2 = {
    .len = ESP_UUID_LEN_16,
    .uuid =
        {
            .uuid16 = REMOTE_READ_CHAR_UUID2,
        },
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid =
        {
            .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
        },
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE};

struct gattc_profile_inst {
	esp_gattc_cb_t gattc_cb;
	uint16_t       gattc_if;
	uint16_t       app_id;
	uint16_t       conn_id;
	uint16_t       service_start_handle;
	uint16_t       service_end_handle;
	uint16_t       char_handle;
	esp_bd_addr_t  remote_bda;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the
 * gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] =
        {
            .gattc_cb = gattc_profile_event_handler,
            .gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is
                                             ESP_GATT_IF_NONE */
        },
};

#endif // GATTC_H
