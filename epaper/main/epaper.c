#include "epaper.h"
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

#define MSG_BUF_LEN 256

MessageBufferHandle_t caption_buf = NULL;

static caption_cfg_t caption_cfg;

static bool rect_is_valid(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end) {
	return (x_start < x_end) && (y_start < y_end) && (x_end <= EPD_7IN5_V2_WIDTH) &&
	       (y_end <= EPD_7IN5_V2_HEIGHT);
}

epaper_err_t caption_init(caption_cfg_t *cfg) {
	if (!rect_is_valid(cfg->x_start, cfg->y_start, cfg->x_end, cfg->y_end)) {
		ESP_LOGE(TAG, "caption_init: Invalid coordinates");
		return EPAPER_ERR;
	}

	if (caption_buf == NULL) {
		// create buffer if not yet
		caption_buf = xMessageBufferCreate(MSG_BUF_LEN);
		if (caption_buf == NULL) {
			ESP_LOGE(TAG, "caption_init: Failed to create caption queue");
			return EPAPER_ERR;
		}
		ESP_LOGI(TAG, "caption_init: Created caption queue");
	} else {
		// clear buffer if it exists
		xMessageBufferReset(caption_buf);
	}
	caption_cfg = *cfg;
	return EPAPER_OK;
}

epaper_err_t caption_clear() {
	Paint_ClearWindows(caption_cfg.x_start, caption_cfg.y_start, caption_cfg.x_end,
	                   caption_cfg.y_end, WHITE);
	EPD_Init_Fast();
	EPD_Display_Part(Image, caption_cfg.x_start, caption_cfg.y_start, caption_cfg.x_end,
	                 caption_cfg.y_end);
	EPD_Init_Part();
	return EPAPER_OK;
}

epaper_err_t caption_append(const char *string) {
	const size_t string_len = strlen(string);

	// split string into space-delimited words
	char word[32];
	size_t word_len = 0;

	// TODO: handle overflow
	for (size_t i = 0; i < string_len + 1; i++) {
		if (string[i] != ' ' && string[i] != '\0') {
			// append letter to word
			word[word_len] = string[i];
			word_len++;
		} else {
			// commit word
			word[word_len] = '\0';
			size_t bytes_sent =
			    xMessageBufferSend(caption_buf, (void *)word, word_len + 1, 0);
			ESP_LOGI(TAG, "caption_append: Appended word \"%s\" (len=%u)", word,
			         word_len);
			if (bytes_sent != word_len + 1) {
				ESP_LOGE(TAG,
				         "caption_append: Failed to send word \"%s\" to buffer "
				         "(expect to send %u bytes, sent %u)",
				         word, word_len + 1, bytes_sent);
				return EPAPER_ERR; // buffer full
			}
			word_len = 0;
		}

	}
	return EPAPER_OK;
}

epaper_err_t caption_display() {
	static UWORD text_row = 0, text_col = 0;

	bool has_updated = false, has_error = false;

	// {min,max}_{x,y} will be updated to clamp to the updated area
	uint16_t min_x = caption_cfg.x_end, max_x = caption_cfg.x_start, min_y = caption_cfg.y_end,
	         max_y = caption_cfg.y_start;

	// TODO: edge case where word length > 32 (unlikely)
	while (xMessageBufferIsEmpty(caption_buf) == pdFALSE) {
		char word[32];

		has_updated = true;

		size_t bytes_recv = xMessageBufferReceive(caption_buf, (void *)word, 32, 0);
		if (bytes_recv == 0) {
			ESP_LOGE(TAG, "caption_display: Failed to receive word from buffer");
			has_error = true;
			break;
		}

		ESP_LOGI(TAG, "caption_display: Received word \"%s\" (len=%u)", word,
		         bytes_recv - 1);

		UWORD word_width_px = strlen(word) * caption_cfg.font->Width;

		if (text_col * caption_cfg.font->Width + word_width_px >= EPD_7IN5_V2_WIDTH) {
			// wrap to next row
			text_col = 0;
			text_row++;
			if (text_row == 8) {
				caption_clear();
				text_row = 0;
			}
		}

		UWORD word_start_x = text_col * caption_cfg.font->Width;
		UWORD word_start_y = text_row * caption_cfg.font->Height;
		UWORD word_end_x = word_start_x + word_width_px;
		UWORD word_end_y = word_start_y + caption_cfg.font->Height;

		min_x = MIN(min_x, word_start_x);
		min_y = MIN(min_y, word_start_y);
		max_x = MAX(max_x, word_end_x);
		max_y = MAX(max_y, word_end_y);

		ESP_LOGI(TAG, "caption_display: Drawing \"%s\" (x=%d:%d, y=%d:%d, col=%d, row=%d)", word,
		         word_start_x, word_end_x, word_start_y, word_end_y, text_col, text_row);

		Paint_DrawString_EN(word_start_x, word_start_y, word, caption_cfg.font, BLACK, WHITE);
		/* EPD_Display_Part(Image, word_start_x, word_start_y, word_end_x, */
				 /* word_end_y); */

		text_col += strlen(word) + 1;
	}

	if (has_updated) {
		ESP_LOGI(TAG, "caption_display: updating screen area (%u, %u) -- (%u, %u)", min_x,
		         min_y, max_x, max_y);
		EPD_Display_Part(Image, min_x, min_y, max_x, max_y);
	}

	if (has_error) {
		return EPAPER_ERR;
	}

	return EPAPER_OK;
}

void epaper_task(void *arg) {
	while (true) {
		caption_display();
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}
