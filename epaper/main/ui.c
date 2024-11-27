#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "epaper.h"
#include "fonts.h"

static const char *TAG = "ui";

epaper_err_t ui_layout_badge(void) {
	ESP_LOGI(TAG, "ui_display_badge: printing \"%s\"", CONFIG_BADGE_NAME);
	Paint_Clear(WHITE);
	// TODO: long names?
	Paint_DrawString_EN(64, 120, CONFIG_BADGE_NAME, &Font48, BLACK, WHITE);
	EPD_Init_Fast();
	EPD_Display(framebuffer);
	return EPAPER_OK;
}

epaper_err_t ui_layout_caption(void) {
	ESP_LOGI(TAG, "ui_display_caption");
	Paint_Clear(WHITE);
	EPD_Init_Fast();
	EPD_Clear();
	EPD_Init_Part();
	return EPAPER_OK;
}
