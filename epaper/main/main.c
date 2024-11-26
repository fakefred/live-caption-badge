/* Includes ------------------------------------------------------------------*/
#include "DEV_Config.h"
#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "epaper.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TAG "main"

void app_main() {
	xTaskCreatePinnedToCore(epaper_task, "epaper", 16 * 1024, NULL, 2, NULL, 0);

	epaper_init();
	epaper_set_state(EPAPER_STATE_CAPTION);

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
		DEV_Delay_ms(50 * chunk_len);
		caption_append(bee[chunk_idx]);
	}

	DEV_Delay_ms(1000);
	epaper_shutdown();
}
