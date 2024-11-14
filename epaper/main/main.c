/* Includes ------------------------------------------------------------------*/
#include "DEV_Config.h"
#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "epaper.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TAG "main"

void app_main() {
	vTaskDelay(pdMS_TO_TICKS(100));

	esp_log_level_set(SPI_TAG, ESP_LOG_NONE);
	esp_log_level_set(EPD_TAG, ESP_LOG_NONE);

	ESP_LOGI(TAG, "EPD demo begin");
	DEV_Module_Init();
	EPD_Init();
	EPD_Clear();
	DEV_Delay_ms(500);

	// Create global framebuffer
	UWORD imagesize = EPD_7IN5_V2_WIDTH_BYTES * EPD_7IN5_V2_HEIGHT;

	Image = (UBYTE *)malloc(imagesize);

	if (Image == NULL) {
		ESP_LOGE(TAG, "Failed to allocate image");
		return;
	}

	Paint_NewImage(Image, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, ROTATE_0, WHITE);
	Paint_Clear(WHITE);

	EPD_Init_Fast();
	EPD_Init_Part();
	DEV_Delay_ms(1000);

	caption_cfg_t caption_cfg = {
		.x_start = 50,
		.y_start = 50,
		.x_end = 750,
		.y_end = 430,
		.font = &Font48,
	};

	caption_init(&caption_cfg);

	xTaskCreatePinnedToCore(epaper_task, "epaper", 16 * 1024, NULL, 2, NULL, 0);

	const char *bee[] = {"According to all",
	                     "known",
	                     "laws of aviation, there",
	                     "is",
	                     "no way a bee should",
	                     "be able to fly. Its wings are too small",
	                     "to get",
	                     "its fat little body off the",
	                     "ground. The bee, of course,",
	                     "flies anyway because",
	                     "bees don't care what humans think is impossible."};

	for (size_t chunk_idx = 0; chunk_idx < sizeof(bee) / sizeof(char *); chunk_idx++) {
		size_t chunk_len = strlen(bee[chunk_idx]);
		caption_append(bee[chunk_idx]);
		DEV_Delay_ms(50 * chunk_len);
	}

	DEV_Delay_ms(500);
	ESP_LOGI(TAG, "EPD going to sleep");
	EPD_Init_Fast();
	EPD_Clear();
	EPD_Sleep();

	while (1) {
		vTaskDelay(1000);
	}
}
