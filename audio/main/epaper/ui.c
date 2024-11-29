#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "bitmap.h"
#include "caption.h"
#include "epaper.h"
#include "fonts.h"

#include <string.h>

static const char *TAG = "ui";

epaper_err_t ui_layout_badge(void) {
	ESP_LOGI(TAG, "ui_layout_badge");
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

	epaper_refresh_area_t refresh_area = {
		.mode = EPAPER_REFRESH_FAST,
		.x_start = 0,
		.y_start = 0,
		.x_end = EPD_7IN5_V2_WIDTH,
		.y_end = EPD_7IN5_V2_HEIGHT,
	};

	xQueueSend(epaper_refresh_queue, &refresh_area, 0);
	return EPAPER_OK;
}

epaper_err_t ui_layout_caption(void) {
	ESP_LOGI(TAG, "ui_layout_caption");
	return caption_clear();
}
