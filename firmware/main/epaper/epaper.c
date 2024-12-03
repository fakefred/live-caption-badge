#include "epaper.h"
#include "DEV_Config.h"
#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "caption.h"
#include "font/fonts.h"
#include "portmacro.h"
#include "ui.h"

#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include <stdint.h>
#include <stdio.h>
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

static bool epaper_is_on = false;
static TaskHandle_t epaper_task_handle;

epaper_ui_t epaper_ui;
SemaphoreHandle_t epaper_sem; // take when epaper is refreshing, give when done
QueueHandle_t epaper_refresh_queue; // queue of areas to refresh

void epaper_task(void *arg);

epaper_err_t epaper_init(void) {
	ESP_LOGI(TAG, "epaper_init");
	assert(!epaper_is_on);

	static bool first_time = true; // first init since startup

	if (first_time) {
		epaper_sem = xSemaphoreCreateBinary();
		xSemaphoreGive(epaper_sem);
		epaper_refresh_queue = xQueueCreate(16, sizeof(epaper_refresh_area_t));
	}
	xQueueReset(epaper_refresh_queue);

	esp_log_level_set(SPI_TAG, ESP_LOG_NONE);

	if (first_time) {
		DEV_Module_Init();
		EPD_Init();
	}
	EPD_Init_Fast();
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
	epaper_is_on = true;
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
	} else if (layout == EPAPER_LAYOUT_PAIR) {
		ui_layout_pair();
	}
	epaper_ui.layout = layout;
	xSemaphoreGive(epaper_sem);
	return EPAPER_OK;
}

epaper_err_t epaper_shutdown(void) {
	ESP_LOGI(TAG, "epaper_shutdown");
	assert(epaper_is_on);
	while (xSemaphoreTake(epaper_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
		ESP_LOGW(TAG, "epaper_shutdown take semaphore timeout");
	}
	EPD_Sleep();
	vTaskSuspend(epaper_task_handle);
	free(framebuffer);
	xSemaphoreGive(epaper_sem);
	epaper_ui.layout = EPAPER_LAYOUT_BADGE;
	epaper_is_on = false;
	return EPAPER_OK;
}

void epaper_task(void *arg) {
	while (true) {
		// Update framebuffer depending on layout
		if (epaper_ui.layout == EPAPER_LAYOUT_CAPTION) {
			caption_display();
		}

		if (xSemaphoreTake(epaper_sem, pdMS_TO_TICKS(5000)) == pdFALSE) {
			ESP_LOGW(TAG, "epaper_task take semaphore timeout");
			continue;
		}

		// only go to sleep if last elem in queue asks us to
		bool need_sleep = false;

		// Transmit refreshed areas to epaper hardware
		while (uxQueueMessagesWaiting(epaper_refresh_queue) > 0) {
			epaper_refresh_area_t refresh_area;

			if (xQueueReceive(epaper_refresh_queue, &refresh_area, 0) == pdFALSE) {
				ESP_LOGE(TAG, "epaper_task: Failed to dequeue");
				continue;
			}

			epaper_refresh_mode_t refresh_mode = refresh_area.mode;
			ESP_LOGI(TAG, "epaper_task: Dequeued mode=%d", refresh_mode);

			need_sleep = false;

			if (refresh_mode & EPAPER_REFRESH_SLOW) {
				EPD_Init();
				EPD_Display(framebuffer);
			} else if (refresh_mode == EPAPER_REFRESH_FAST) {
				EPD_Init_Fast();
				EPD_Display(framebuffer);
			} else if (refresh_mode == EPAPER_REFRESH_PARTIAL) {
				EPD_Init_Part();
				EPD_Display_Part(framebuffer, refresh_area.x_start,
				                 refresh_area.y_start, refresh_area.x_end,
				                 refresh_area.y_end);
			} else if (refresh_mode == EPAPER_REFRESH_CLEAR) {
				EPD_Init_Fast();
				EPD_Clear();
			} else if (refresh_mode == EPAPER_REFRESH_SLEEP) {
				EPD_Init();
				EPD_Display(framebuffer);
				need_sleep = true;
			}
		}

		if (need_sleep) {
			EPD_Sleep();
		}

		xSemaphoreGive(epaper_sem);
	}
}
