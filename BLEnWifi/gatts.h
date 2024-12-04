#ifndef GATTS_H
#define GATTS_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "gatts_table_creat_demo.h"
#include "esp_gatt_common_api.h"

#define GATTS_TABLE_TAG "GATTS_TABLE_DEMO"

#define PROFILE_NUM                 1
#define PROFILE_APP_IDX             0
#define ESP_APP_ID                  0x55
//#define SAMPLE_DEVICE_NAME          "LIVE_CAPTION_BADGE_1"
#define SVC_INST_ID                 0

/* The max length of characteristic value. When the GATT client performs a write or prepare write operation,
*  the data length must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
*/
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500
#define PREPARE_BUF_MAX_SIZE        1024
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

typedef struct {
    uint8_t                 *prepare_buf;
    int                     prepare_len;
} prepare_type_env_t;

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

void example_prepare_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);

void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);

void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);


void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

void gatts_init(void);

#endif // GATTS_H