#include "epaper.h"
#include "DEV_Config.h"
#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "esp_err.h"
#include "esp_log.h"
#include "fonts.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "portmacro.h"
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

epaper_state_t epaper_state = EPAPER_STATE_UNINITIALIZED;

epaper_err_t epaper_init(void) {
	vTaskDelay(pdMS_TO_TICKS(100));

	esp_log_level_set(SPI_TAG, ESP_LOG_NONE);

	ESP_LOGI(TAG, "epaper_task begin");

	DEV_Module_Init();
	EPD_Init();
	EPD_Clear();
	DEV_Delay_ms(500);

	// Create global framebuffer
	UWORD imagesize = EPD_7IN5_V2_WIDTH_BYTES * EPD_7IN5_V2_HEIGHT;

	framebuffer = (UBYTE *)malloc(imagesize);

	if (framebuffer == NULL) {
		ESP_LOGE(TAG, "Failed to allocate image");
		return EPAPER_ERR;
	}

	Paint_NewImage(framebuffer, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, ROTATE_0, WHITE);
	Paint_Clear(WHITE);

	EPD_Init_Fast();
	EPD_Init_Part();
	DEV_Delay_ms(1000);

	caption_cfg_t caption_cfg = {
		.x_start = 0,
		.y_start = 120,
		.x_end = 800,
		.y_end = 480,
		.font = &Font48,
	};

	caption_init(&caption_cfg);

	epaper_state = EPAPER_STATE_IDLE;

	return EPAPER_OK;
}

void epaper_task(void *arg) {
	static epaper_state_t prev_state = EPAPER_STATE_UNINITIALIZED;
	while (true) {
		if (epaper_state == EPAPER_STATE_CAPTION) {
			caption_display();
		} else if (epaper_state == EPAPER_STATE_SHUTDOWN &&
		           prev_state != EPAPER_STATE_SHUTDOWN) {
		}
		prev_state = epaper_state;
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

epaper_err_t epaper_set_state(epaper_state_t new_state) {
	if (epaper_state == EPAPER_STATE_UNINITIALIZED) {
		ESP_LOGE(TAG, "epaper_set_state: epaper is uninitialized");
		return EPAPER_ERR;
	}
	epaper_state = new_state;
	return EPAPER_OK;
}

epaper_err_t epaper_shutdown(void) {
	ESP_LOGI(TAG, "epaper_shutdown: going to sleep");
	EPD_Init_Fast();
	EPD_Clear();
	EPD_Sleep();
	return EPAPER_OK;
}
