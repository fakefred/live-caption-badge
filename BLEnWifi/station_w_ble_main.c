/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
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

#include "wifi.h"
#include "gatts.h"
#include "gattc.h"

const char deviceName[] = "LIVE_CAPTION_BADGE_2";

static const char *TAG = "main";
char ipAddrComplete [20] = {0};
const char name[20] = "Larry Page";
static user_t userList[MAX_DEVICES];
user_t userBuffer;
QueueHandle_t q;

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Start WiFi Connection!");
    wifi_init();

    ESP_LOGI(TAG, "GATTS setup start!");

    gatts_init();
    ESP_LOGI(TAG, "Start advertising name and IP address.");

    ESP_LOGI(TAG, "Start Scanning for target device.");
    q = xQueueCreate(1, sizeof(user_t));
    ret = gattc_start();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "GATTC setup failed.");
    }
    // while(1){
    //     if( xQueueReceive( q, &( userBuffer ),( TickType_t ) 10 ) == pdPASS ){
    //         /* xRxedStructure now contains a copy of xMessage. */
    //         ESP_LOGI(TAG, "received user name: %s", userBuffer.name);
    //         ESP_LOGI(TAG, "received user IP: %s", userBuffer.ip);
    //     }else{
    //         ESP_LOGI(TAG, "empty queue");
    //     }
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    vTaskDelay(pdMS_TO_TICKS(8000));
    ret = gattc_start();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "GATTC setup failed.");
    }
}
