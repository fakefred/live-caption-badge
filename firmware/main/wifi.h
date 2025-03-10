#ifndef WIFI_H
#define WIFI_H

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "gatts_table_creat_demo.h"

extern char ipAddrComplete[20];
extern bool wifiIsConnected;

void        intToStr(int num, char *str);
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                          void *event_data);
void        wifi_init(void);

#endif // WIFI_H
