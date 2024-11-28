#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "bitmap.h"
#include "epaper.h"
#include "fonts.h"

#include <string.h>

static const char *TAG = "ui";

epaper_err_t ui_layout_badge(void) {
	ESP_LOGI(TAG, "ui_display_badge");
	Paint_Clear(WHITE);
	Paint_DrawImage(UMich_Logo_Bitmap, 0, 30, UMICH_LOGO_BITMAP_WIDTH,
	                UMICH_LOGO_BITMAP_HEIGHT);
	// TODO: long names?
	Paint_DrawString_EN(64, 180, CONFIG_PARTICIPANT_NAME, &Font48, BLACK, WHITE);
	Paint_DrawString_EN(64, 240, CONFIG_PARTICIPANT_PRONOUNS, &Font32, BLACK, WHITE);
	Paint_DrawString_EN(32, 400, CONFIG_PARTICIPANT_AFFILIATION, &Font32, BLACK, WHITE);
	size_t role_len = strlen(CONFIG_PARTICIPANT_ROLE);
	UWORD x_start = 768 - role_len * Font32.Width;
	Paint_DrawString_EN(x_start, 400, CONFIG_PARTICIPANT_ROLE, &Font32, BLACK, WHITE);
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
