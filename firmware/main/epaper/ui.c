#include "DEV_Config.h"
#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "bitmap/bitmaps.h"
#include "caption.h"
#include "epaper.h"
#include "font/fonts.h"

#include <string.h>

static const char *TAG = "ui";

void draw_string_large(UWORD x_start, UWORD y_start, const char *str) {
	Paint_DrawString_EN(x_start, y_start, str, &Font48, BLACK, WHITE);
}

void draw_string_medium(UWORD x_start, UWORD y_start, const char *str) {
	Paint_DrawString_EN(x_start, y_start, str, &Font32, BLACK, WHITE);
}

void draw_bitmap(UWORD x_start, UWORD y_start, const bitmap_t *bitmap) {
	Paint_DrawImage(bitmap->table, x_start, y_start, bitmap->width, bitmap->height);
}

epaper_err_t ui_layout_badge(void) {
	ESP_LOGI(TAG, "ui_layout_badge");
	Paint_Clear(WHITE);
	draw_bitmap(0, 30, &UMICH_LOGO);
	// TODO: long names?
	draw_string_large(64, 180, CONFIG_PARTICIPANT_NAME);
	draw_string_medium(64, 240, CONFIG_PARTICIPANT_PRONOUNS);
	draw_string_medium(32, 400, CONFIG_PARTICIPANT_AFFILIATION);
	// HACK: right align
	size_t role_len = strlen(CONFIG_PARTICIPANT_ROLE);
	UWORD x_start = 768 - role_len * Font32.Width;
	draw_string_medium(x_start, 400, CONFIG_PARTICIPANT_ROLE);

	epaper_refresh_area_t refresh_area = {
		.mode = EPAPER_REFRESH_SLEEP,
	};

	xQueueSend(epaper_refresh_queue, &refresh_area, 0);
	return EPAPER_OK;
}

epaper_err_t ui_layout_caption(void) {
	ESP_LOGI(TAG, "ui_layout_caption");
	return caption_clear();
}

epaper_err_t ui_layout_pair(void) {
	ESP_LOGI(TAG, "ui_layout_pair");
	Paint_Clear(WHITE);

	Paint_SetRotate(ROTATE_180);

	Paint_DrawString_EN(64, 280, "Pair with {NAME}?", &Font32, BLACK, WHITE);

	Paint_SetRotate(ROTATE_0);

	Paint_DrawRectangle(10, 236, 790, 244, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
	Paint_DrawString_EN(64, 280, "Hello, my name is", &Font32, BLACK, WHITE);
	Paint_DrawString_EN(64, 350, CONFIG_PARTICIPANT_NAME, &Font48, BLACK, WHITE);
	
	epaper_refresh_area_t refresh_area = {
		.mode = EPAPER_REFRESH_FAST,
	};

	xQueueSend(epaper_refresh_queue, &refresh_area, 0);
	return EPAPER_OK;
}

epaper_err_t ui_layout_wifi_connecting(void) {
	ESP_LOGI(TAG, "ui_layout_wifi_connecting");
	Paint_Clear(WHITE);
	draw_bitmap(32, 112, &WIFI_LOGO);
	draw_string_medium(320, 220, "Connecting to Wi-Fi...");

	epaper_refresh_area_t refresh_area = {
		.mode = EPAPER_REFRESH_FAST,
	};

	xQueueSend(epaper_refresh_queue, &refresh_area, 0);
	return EPAPER_OK;
}

epaper_err_t ui_layout_wifi_connected(void) {
	ESP_LOGI(TAG, "ui_layout_wifi_connected");
	Paint_Clear(WHITE);
	draw_bitmap(32, 112, &WIFI_LOGO);
	draw_string_medium(320, 220, "Connected to Wi-Fi!");

	epaper_refresh_area_t refresh_area = {
		.mode = EPAPER_REFRESH_FAST,
	};

	xQueueSend(epaper_refresh_queue, &refresh_area, 0);
	return EPAPER_OK;
}

epaper_err_t ui_layout_wifi_disconnected(void) {
	ESP_LOGI(TAG, "ui_layout_wifi_disconnected");
	Paint_Clear(WHITE);
	draw_bitmap(32, 112, &NO_WIFI_LOGO);
	draw_string_medium(320, 220, "Wi-Fi disconnected");

	epaper_refresh_area_t refresh_area = {
		.mode = EPAPER_REFRESH_FAST,
	};

	xQueueSend(epaper_refresh_queue, &refresh_area, 0);
	return EPAPER_OK;
}
