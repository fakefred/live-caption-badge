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
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TAG "epaper"

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

static bool epaper_is_initialized = false;
epaper_ui_t epaper_ui;
TaskHandle_t epaper_task_handle;
SemaphoreHandle_t epaper_sem; // take when epaper is refreshing, give when done

void epaper_task(void *arg);

epaper_err_t epaper_init(void) {
	ESP_LOGI(TAG, "epaper_init");
	assert(!epaper_is_initialized);

	static bool first_time = true; // first init since startup

	if (first_time) {
		epaper_sem = xSemaphoreCreateBinary();
		xSemaphoreGive(epaper_sem);
	}

	esp_log_level_set(SPI_TAG, ESP_LOG_NONE);

	if (first_time) {
		DEV_Module_Init();
	}
	EPD_Init();
	EPD_Clear();
	DEV_Delay_ms(500);

	// Create global framebuffer
	// framebuffer is defined in GUI_Paint.c
	UWORD imagesize = EPD_7IN5_V2_WIDTH_BYTES * EPD_7IN5_V2_HEIGHT;
	framebuffer = (UBYTE *)malloc(imagesize);
	if (framebuffer == NULL) {
		ESP_LOGE(TAG, "Failed to allocate image");
		return EPAPER_ERR;
	}
	Paint_NewImage(framebuffer, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, ROTATE_0, WHITE);
	Paint_Clear(WHITE);

	DEV_Delay_ms(1000);

	// Initialize caption layout on screen
	caption_cfg_t caption_cfg = {
		.x_start = 0,
		.y_start = 0,
		.x_end = 800,
		.y_end = 480,
		.font = &Font48,
	};
	caption_init(&caption_cfg);

	epaper_ui = (epaper_ui_t){
		.layout = EPAPER_LAYOUT_BADGE,
	};

	if (first_time) {
		// priority set to 1 because ESP32-C6-DevKitM has one core only
		// so app_main and epaper_task have to round-robin
		xTaskCreate(epaper_task, "epaper", 16 * 1024, NULL, 1, &epaper_task_handle);
	} else {
		vTaskResume(epaper_task_handle);
	}

	first_time = false;
	epaper_is_initialized = true;
	return EPAPER_OK;
}

epaper_err_t epaper_ui_set_layout(epaper_layout_t layout) {
	ESP_LOGI(TAG, "epaper_ui_set_layout: layout = %d", layout);
	while (xSemaphoreTake(epaper_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
		ESP_LOGW(TAG, "epaper_ui_set_layout take semaphore timeout");
	}
	if (layout == EPAPER_LAYOUT_BADGE) {
		ui_layout_badge();
	} else if (layout == EPAPER_LAYOUT_CAPTION) {
		ui_layout_caption();
	}
	epaper_ui.layout = layout;
	/* DEV_Delay_ms(1000); */
	xSemaphoreGive(epaper_sem);
	return EPAPER_OK;
}

epaper_err_t epaper_shutdown(void) {
	ESP_LOGI(TAG, "epaper_shutdown");
	assert(epaper_is_initialized);
	while (xSemaphoreTake(epaper_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
		ESP_LOGW(TAG, "epaper_shutdown take semaphore timeout");
	}
	EPD_Init_Fast();
	EPD_Clear();
	EPD_Sleep();
	vTaskSuspend(epaper_task_handle);
	free(framebuffer);
	xSemaphoreGive(epaper_sem);
	epaper_ui.layout = EPAPER_LAYOUT_BADGE;
	epaper_is_initialized = false;
	return EPAPER_OK;
}

void epaper_task(void *arg) {
	while (true) {
		if (xSemaphoreTake(epaper_sem, pdMS_TO_TICKS(5000)) == pdFALSE) {
			ESP_LOGW(TAG, "epaper_task take semaphore timeout");
			continue;
		}
		if (epaper_ui.layout == EPAPER_LAYOUT_CAPTION) {
			caption_display();
		}
		xSemaphoreGive(epaper_sem);

		if (epaper_ui.layout == EPAPER_LAYOUT_CAPTION) {

		} else {
			DEV_Delay_ms(1000);
		}
	}
}
