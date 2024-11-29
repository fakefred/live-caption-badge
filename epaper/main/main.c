/* Includes ------------------------------------------------------------------*/
#include "DEV_Config.h"
#include "caption.h"
#include "epaper.h"

#include "freertos/idf_additions.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TAG "main"

void app_main() {
	DEV_Delay_ms(100); // wait for power to stablize

	const char *bee[] = {
		"According to all",
		"known",
		"laws of aviation, there",
		"is",
		"no way a bee should",
		"be able to fly.",
		"Its wings are too small",
		"to get",
		"its fat little body off the",
		"ground. The bee, of course,",
		"flies anyway because",
		"bees don't care what",
		"humans think is impossible.",
	};

	epaper_init();
	DEV_Delay_ms(500);
	epaper_ui_set_layout(EPAPER_LAYOUT_BADGE);
        DEV_Delay_ms(2000);

	epaper_ui_set_layout(EPAPER_LAYOUT_CAPTION);

	for (size_t chunk_idx = 0; chunk_idx < sizeof(bee) / sizeof(char *); chunk_idx++) {
		size_t chunk_len = strlen(bee[chunk_idx]);
		DEV_Delay_ms(50 * chunk_len);
		caption_append(bee[chunk_idx]);
	}

	DEV_Delay_ms(2000);

        epaper_ui_set_layout(EPAPER_LAYOUT_BADGE);
	DEV_Delay_ms(2000);
	epaper_shutdown();
	vTaskDelete(NULL);
}
