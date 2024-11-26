#include "epaper.h"
#include "DEV_Config.h"
#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "esp_log.h"
#include "fonts.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include <stdint.h>
#include <string.h>
#include <strings.h>

#define TAG "epaper"

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

void epaper_task(void *arg) {
	while (true) {
		caption_display();
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}
