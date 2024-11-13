/* Includes ------------------------------------------------------------------*/
#include "DEV_Config.h"
#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "esp_log.h"
#include "fonts.h"
#include "freertos/idf_additions.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TAG "epaper"

#define FONT Font48

/* Entry point
 * ----------------------------------------------------------------*/
void epaper_task(void *arg) {
	ESP_LOGI(TAG, "EPD demo begin");
	DEV_Module_Init();
	EPD_Init();
	EPD_Clear();
	DEV_Delay_ms(500);

	// Create a new image
	UWORD imagesize =
	    ((EPD_7IN5_V2_WIDTH % 8 == 0) ? (EPD_7IN5_V2_WIDTH / 8)
	                                  : (EPD_7IN5_V2_WIDTH / 8 + 1)) *
	    EPD_7IN5_V2_HEIGHT;

	UBYTE *image = (UBYTE *)malloc(imagesize);
	if (image == NULL) {
		ESP_LOGE(TAG, "Failed to allocate image");
		return;
	}

	EPD_Init_Fast();
	EPD_Init_Part();
	DEV_Delay_ms(1000);

	UWORD text_row = 0, text_col = 0;

	const char *bee[] = {
	    "According", "to",         "all",    "known",   "laws",
	    "of",        "aviation,",  "there",  "is",      "no",
	    "way",       "a",          "bee",    "should",  "be",
	    "able",      "to",         "fly.",   "Its",     "wings",
	    "are",       "too",        "small",  "to",      "get",
	    "its",       "fat",        "little", "body",    "off",
	    "the",       "ground.",    "The",    "bee,",    "of",
	    "course,",   "flies",      "anyway", "because", "bees",
	    "don't",     "care",       "what",   "humans",  "think",
	    "is",        "impossible."};

	for (size_t word_idx = 0; word_idx < sizeof(bee) / sizeof(char *); word_idx++) {
		const char *word = bee[word_idx];
		UWORD word_width_px = strlen(word) * FONT.Width;

		if (text_col * FONT.Width + word_width_px >= EPD_7IN5_V2_WIDTH) {
			// wrap to next row
			text_col = 0;
			text_row++;
			if (text_row == 8) {
				break;
			}
		}

		UWORD word_start_x = text_col * FONT.Width;
		UWORD word_start_y = text_row * FONT.Height;
		UWORD word_end_x = word_start_x + word_width_px;
		UWORD word_end_y = word_start_y + FONT.Height;

		ESP_LOGI(TAG,
		         "Drawing \"%s\" (x=%d:%d, y=%d:%d, col=%d, row=%d)",
		         word, word_start_x, word_end_x, word_start_y,
		         word_end_y, text_col, text_row);

		Paint_NewImage(image, word_width_px, FONT.Height, 0, WHITE);
		Paint_SelectImage(image);
		Paint_Clear(WHITE);
		Paint_DrawString_EN(0, 0, word, &FONT, BLACK, WHITE);
		EPD_Display_Part(image, word_start_x, word_start_y, word_end_x,
		                 word_end_y);

		text_col += strlen(word) + 1;
	}

	EPD_Sleep();
	while (1) {
		vTaskDelay(1000);
	}
}

void app_main() {
	vTaskDelay(pdMS_TO_TICKS(100));
	xTaskCreate(epaper_task, "epaper", 16 * 1024, NULL, 2, NULL);
}
