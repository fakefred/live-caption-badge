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

#define FONT_WIDTH Font20.Width
/* #define FONT_WIDTH 16 */
#define FONT_HEIGHT Font20.Height

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
	    "According", "to",       "all",         "known",
	    "laws",      "of",       "aviation,",   "there",
	    "is",        "no",       "way",         "a",
	    "bee",       "should",   "be",          "able",
	    "to",        "fly.",     "Its",         "wings",
	    "are",       "too",      "small",       "to",
	    "get",       "its",      "fat",         "little",
	    "body",      "off",      "the",         "ground.",
	    "The",       "bee,",     "of",          "course,",
	    "flies",     "anyway",   "because",     "bees",
	    "don't",     "care",     "what",        "humans",
	    "think",     "is",       "impossible.", "Yellow,",
	    "black.",    "Yellow,",  "black.",      "Yellow,",
	    "black.",    "Yellow,",  "black.",      "Ooh,",
	    "black",     "and",      "yellow!",     "Let's",
	    "shake",     "it",       "up",          "a",
	    "little.",   "Barry!",   "Breakfast",   "is",
	    "ready!",    "Coming!",  "Hang",        "on",
	    "a",         "second.",  "Hello?",      "Barry?",
	    "Adam?",     "Can",      "you",         "believe",
	    "this",      "is",       "happening?",  "I",
	    "can't.",    "I'll",     "pick",        "you",
	    "up.",       "Looking",  "sharp.",      "Use",
	    "the",       "stairs,",  "Your",        "father",
	    "paid",      "good",     "money",       "for",
	    "those.",    "Sorry.",   "I'm",         "excited.",
	    "Here's",    "the",      "graduate.",   "We're",
	    "very",      "proud",    "of",          "you,",
	    "son.",      "A",        "perfect",     "report",
	    "card,",     "all",      "B's.",        "Very",
	    "proud.",    "Ma!",      "I",           "got",
	    "a",         "thing",    "going",       "here.",
	    "You",       "got",      "lint",        "on",
	    "your",      "fuzz.",    "Ow!",         "That's",
	    "me!",       "Wave",     "to",          "us!",
	    "We'll",     "be",       "in",          "row",
	    "118,000.",  "Bye!",     "Barry,",      "I",
	    "told",      "you,",     "stop",        "flying",
	    "in",        "the",      "house!",      "Hey,",
	    "Adam.",     "Hey,",     "Barry.",      "Is",
	    "that",      "fuzz",     "gel?",        "A",
	    "little.",   "Special",  "day,",        "graduation.",
	    "Never",     "thought",  "I'd",         "make",
	    "it.",       "Three",    "days",        "grade",
	    "school,",   "three",    "days",        "high",
	    "school.",   "Those",    "were",        "awkward.",
	    "Three",     "days",     "college.",    "I'm",
	    "glad",      "I",        "took",        "a",
	    "day",       "and",      "hitchhiked",  "around",
	    "The",       "Hive.",    "You",         "did",
	    "come",      "back",     "different.",  "Hi,",
	    "Barry.",    "Artie,",   "growing",     "a",
	    "mustache?", "Looks",    "good.",       "Hear",
	    "about",     "Frankie?", "Yeah.",       "You",
	    "going",     "to",       "the",         "funeral?",
	    "No,",       "I'm",      "not",         "going.",
	    "Everybody", "knows,",   "sting",       "someone,",
	    "you",       "die.",     "Don't",       "waste",
	    "it",        "on",       "a",           "squirrel.",
	    "Such",      "a",        "hothead."};

	for (size_t word_idx = 0; word_idx < sizeof(bee) / sizeof(char *); word_idx++) {
		const char *word = bee[word_idx];
		UWORD word_width_px = (strlen(word) + 1) * FONT_WIDTH;

		if (text_col * FONT_WIDTH + word_width_px >= EPD_7IN5_V2_WIDTH) {
			// wrap to next row
			text_col = 0;
			text_row++;
		}

		UWORD word_start_x = text_col * FONT_WIDTH;
		UWORD word_start_y = text_row * FONT_HEIGHT;
		UWORD word_end_x = word_start_x + word_width_px;
		UWORD word_end_y = word_start_y + FONT_HEIGHT;

		ESP_LOGI(TAG,
		         "Drawing \"%s\" (x=%d:%d, y=%d:%d, col=%d, row=%d)",
		         word, word_start_x, word_end_x, word_start_y,
		         word_end_y, text_col, text_row);

		Paint_NewImage(image, word_width_px, FONT_HEIGHT, 0, WHITE);
		Paint_SelectImage(image);
		Paint_Clear(WHITE);
		Paint_DrawString_EN(0, 0, word, &Font20, BLACK, WHITE);
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
