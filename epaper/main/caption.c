#include "epaper.h"
#include "epaper.h"
#include "DEV_Config.h"
#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "esp_log.h"
#include "fonts.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include <stdint.h>
#include <string.h>
#include <strings.h>

#define MSG_BUF_LEN 256
#define MAX_WORD_LEN 32 // including null terminator

static const char *TAG = "caption";

MessageBufferHandle_t caption_buf = NULL;

static caption_cfg_t cfg;

static UWORD text_row = 0, text_col = 0;
static UWORD total_text_rows = 0, total_text_cols = 0; // # rows/cols we can display in the area

static bool rect_is_valid(UWORD x_start, UWORD y_start, UWORD x_end, UWORD y_end) {
	return (x_start < x_end) && (y_start < y_end) && (x_end <= EPD_7IN5_V2_WIDTH) &&
	       (y_end <= EPD_7IN5_V2_HEIGHT);
}

epaper_err_t caption_init(caption_cfg_t *init_cfg) {
	if (!rect_is_valid(init_cfg->x_start, init_cfg->y_start, init_cfg->x_end, init_cfg->y_end)) {
		ESP_LOGE(TAG, "caption_init: Invalid coordinates");
		return EPAPER_ERR;
	}

	if (init_cfg->font == NULL) {
		ESP_LOGE(TAG, "caption_init: Font must not be NULL");
		return EPAPER_ERR;
	}

	UWORD rows = (init_cfg->y_end - init_cfg->y_start) / init_cfg->font->Height;
	UWORD cols = (init_cfg->x_end - init_cfg->x_start) / init_cfg->font->Width;

	if (cols < 8 || rows < 2) {
		ESP_LOGE(TAG, "caption_init: Area too small");
		return EPAPER_ERR;
	}

	if (caption_buf == NULL) {
		// create buffer if not yet
		caption_buf = xMessageBufferCreate(MSG_BUF_LEN);
		if (caption_buf == NULL) {
			ESP_LOGE(TAG, "caption_init: Failed to create caption buffer");
			return EPAPER_ERR;
		}
		ESP_LOGI(TAG, "caption_init: Created caption buffer");
	} else {
		// clear buffer if it exists
		xMessageBufferReset(caption_buf);
		ESP_LOGI(TAG, "caption_init: Reset caption buffer");
	}

	cfg = *init_cfg;
	total_text_rows = rows;
	total_text_cols = cols;
	ESP_LOGI(TAG, "caption_init: Initialized caption area with %u columns, %u rows",
	         total_text_cols, total_text_rows);
	return EPAPER_OK;
}

epaper_err_t caption_clear() {
	Paint_ClearWindows(cfg.x_start, cfg.y_start, cfg.x_end, cfg.y_end, WHITE);
	EPD_Init_Fast();
	EPD_Init_Part();
	EPD_Display_Part(Image, cfg.x_start, cfg.y_start, cfg.x_end, cfg.y_end);
	return EPAPER_OK;
}

epaper_err_t caption_append(const char *string) {
	const size_t string_len = strlen(string);

	// split string into space-delimited words
	char   word[MAX_WORD_LEN];
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
	bool has_update = false, // there is new text to print
	    has_error = false,   // an error was encountered
	    need_clear = false;  // a part of the caption area needs to be cleared

	// {min,max}_{x,y} will be updated to clamp to the updated area
	UWORD min_x = cfg.x_end, max_x = cfg.x_start, min_y = cfg.y_end, max_y = cfg.y_start;

	UWORD clear_row_start = 0, clear_row_end = 0;

	// TODO: edge case where word length > MAX_WORD_LEN (unlikely)
	while (xMessageBufferIsEmpty(caption_buf) == pdFALSE) {
		char word[MAX_WORD_LEN];

		has_update = true;

		size_t bytes_recv =
		    xMessageBufferReceive(caption_buf, (void *)word, MAX_WORD_LEN, 0);
		if (bytes_recv == 0) {
			ESP_LOGE(TAG, "caption_display: Failed to receive word from buffer");
			has_error = true;
			break;
		}

		size_t word_len = strlen(word);

		if (text_col + word_len >= total_text_cols) {
			// wrap to next row
			text_col = 0;
			text_row = (text_row + 1) % total_text_rows;

			// erase area we are going to print on soon
			if (text_row == total_text_rows / 2 - 1) {
				clear_row_start = total_text_rows / 2;
				clear_row_end = total_text_rows;
				need_clear = true;
			} else if (text_row == total_text_rows - 1) {
				clear_row_start = 0;
				clear_row_end = total_text_rows / 2;
				need_clear = true;
			}
		}

		UWORD word_width_px = word_len * cfg.font->Width;
		UWORD word_start_x = cfg.x_start + text_col * cfg.font->Width;
		UWORD word_start_y = cfg.y_start + text_row * cfg.font->Height;
		UWORD word_end_x = word_start_x + word_width_px;
		UWORD word_end_y = word_start_y + cfg.font->Height;

		min_x = MIN(min_x, word_start_x);
		min_y = MIN(min_y, word_start_y);
		max_x = MAX(max_x, word_end_x);
		max_y = MAX(max_y, word_end_y);

		ESP_LOGI(TAG, "caption_display: Drawing \"%s\" (x=%d:%d, y=%d:%d, col=%d, row=%d)",
		         word, word_start_x, word_end_x, word_start_y, word_end_y, text_col,
		         text_row);

		Paint_DrawString_EN(word_start_x, word_start_y, word, cfg.font, BLACK, WHITE);

		text_col += strlen(word) + 1;

		if (need_clear) {
			break;
		}
	}

	UWORD clear_y_start = cfg.y_start + clear_row_start * cfg.font->Height,
	      clear_y_end = cfg.y_start + clear_row_end * cfg.font->Height;

	if (has_update) {
		if (need_clear) {
			ESP_LOGI(TAG, "caption_display: Updating entire caption area");
			Paint_ClearWindows(cfg.x_start, clear_y_start, cfg.x_end, clear_y_end,
			                   WHITE);
			EPD_Display_Part(Image, cfg.x_start, cfg.y_start, cfg.x_end, cfg.y_end);
		} else {
			ESP_LOGI(TAG, "caption_display: Updating screen area (%u, %u) -- (%u, %u)",
			         min_x, min_y, max_x, max_y);
			EPD_Display_Part(Image, min_x, min_y, max_x, max_y);
		}
	}

	if (has_error) {
		return EPAPER_ERR;
	}

	return EPAPER_OK;
}
