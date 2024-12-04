#include "DEV_Config.h"
#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "bitmap/bitmaps.h"
#include "board.h"
#include "board_def.h"
#include "caption.h"
#include "epaper.h"
#include "font/fonts.h"

#include <stdio.h>
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

void draw_button(int button_id, const bitmap_t *bitmap) {
	// bitmap must be rotated 180 degrees.
	UWORD x_start = 0;
	if (button_id == BUTTON_ID_1) {
		x_start = 270;
	} else if (button_id == BUTTON_ID_2) {
		x_start = 190;
	} else if (button_id == BUTTON_ID_3) {
		x_start = 110;
	} else {
		ESP_LOGE(TAG, "draw_button: Invalid button_id %d", button_id);
		return;
	}

	draw_bitmap(x_start, 10, bitmap);
}

epaper_err_t ui_layout_badge(void) {
	ESP_LOGI(TAG, "ui_layout_badge");

	Paint_Clear(WHITE);
	draw_bitmap(0, 90, &UMICH_LOGO);
	// TODO: long names?
	draw_string_large(64, 240, CONFIG_PARTICIPANT_NAME);
	draw_string_medium(64, 300, CONFIG_PARTICIPANT_PRONOUNS);
	draw_string_medium(32, 400, CONFIG_PARTICIPANT_AFFILIATION);
	// HACK: right align
	size_t role_len = strlen(CONFIG_PARTICIPANT_ROLE);
	UWORD x_start = 768 - role_len * Font32.Width;
	draw_string_medium(x_start, 400, CONFIG_PARTICIPANT_ROLE);

	draw_button(BUTTON_ID_1, &UNMUTE_LOGO);

	epaper_refresh_area_t refresh_area = {
		.mode = EPAPER_REFRESH_SLEEP,
	};

	xQueueSend(epaper_refresh_queue, &refresh_area, 0);
	caption_enabled = false;
	return EPAPER_OK;
}

epaper_err_t ui_layout_caption(void) {
	ESP_LOGI(TAG, "ui_layout_caption");

	draw_button(BUTTON_ID_1, &MUTE_LOGO);
	caption_enabled = true;
	return caption_clear();
}

static void print_name(void) {
	Paint_DrawRectangle(10, 236, 790, 244, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
	draw_string_medium(64, 280, "Hello, my name is");
	draw_string_large(64, 350, CONFIG_PARTICIPANT_NAME);
}

epaper_err_t ui_layout_pair_searching(void) {
	ESP_LOGI(TAG, "ui_layout_pair_searching");
	Paint_Clear(WHITE);

	draw_bitmap(540, 40, &PAIR_LOGO);

	Paint_SetRotate(ROTATE_180);
	draw_string_medium(300, 280, "Searching for");
	draw_string_medium(300, 350, "nearby badges...");
	Paint_SetRotate(ROTATE_0);

	print_name();

	epaper_refresh_area_t refresh_area = {
		.mode = EPAPER_REFRESH_FAST,
	};
	xQueueSend(epaper_refresh_queue, &refresh_area, 0);
	return EPAPER_OK;
}

epaper_err_t ui_layout_pair_confirm(const char *peer_name) {
	ESP_LOGI(TAG, "ui_layout_pair_confirm");
	Paint_Clear(WHITE);

	draw_bitmap(540, 40, &PAIR_LOGO);

	Paint_SetRotate(ROTATE_180);
	if (peer_name != NULL) {
		draw_string_medium(300, 280, "Pair with this badge?");
		draw_string_large(300, 350, peer_name);
		draw_button(BUTTON_ID_1, &CHECK_LOGO);
		draw_button(BUTTON_ID_2, &CROSS_LOGO);
	} else { // HACK
		draw_string_medium(300, 280, "No nearby badges found.");
	}
	Paint_SetRotate(ROTATE_0);

	print_name();
	
	epaper_refresh_area_t refresh_area = {
		.mode = EPAPER_REFRESH_FAST,
	};
	xQueueSend(epaper_refresh_queue, &refresh_area, 0);
	return EPAPER_OK;
}

epaper_err_t ui_layout_pair_pending(const char *peer_name) {
	ESP_LOGI(TAG, "ui_layout_pair_confirm");
	Paint_Clear(WHITE);

	draw_bitmap(540, 40, &PAIR_LOGO);

	Paint_SetRotate(ROTATE_180);
	assert(peer_name != NULL);
	draw_string_medium(300, 280, "Requesting to pair...");
	draw_string_large(300, 350, peer_name);
	Paint_SetRotate(ROTATE_0);

	print_name();
	
	epaper_refresh_area_t refresh_area = {
		.mode = EPAPER_REFRESH_FAST,
	};
	xQueueSend(epaper_refresh_queue, &refresh_area, 0);
	return EPAPER_OK;
}

epaper_err_t ui_layout_pair_result(const char *peer_name) {
	ESP_LOGI(TAG, "ui_layout_pair_confirm");
	Paint_Clear(WHITE);

	draw_bitmap(540, 40, &PAIR_LOGO);

	Paint_SetRotate(ROTATE_180);
	if (peer_name != NULL) {
		draw_string_medium(300, 280, "You are paired with");
		draw_string_large(300, 350, peer_name);
	} else { // HACK
		draw_string_medium(300, 280, "Pairing failed");
	}
	Paint_SetRotate(ROTATE_0);

	print_name();
	
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
	draw_string_medium(320, 220, "Wi-Fi connected");

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
