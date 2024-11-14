#pragma once

#include "fonts.h"
#include "freertos/idf_additions.h"
#include <stdint.h>

extern MessageBufferHandle_t caption_buf;

typedef enum {
	EPAPER_OK,
	EPAPER_ERR,
} epaper_err_t;

typedef struct {
	// bounding box position, in pixels
	uint16_t x_start, y_start, x_end, y_end;
	sFONT *font;
} caption_cfg_t;

/*
 * Resets data structures for caption.
 */
epaper_err_t caption_init(caption_cfg_t *cfg);

/*
 * Clears caption area on screen, so that the next chunk of text will be printed in the top left
 * corner.
 */
epaper_err_t caption_clear();

/*
 * Schedule `string` to be printed in caption area on screen.
 *
 * string: null-terminated C string
 */
epaper_err_t caption_append(const char *string);

epaper_err_t caption_display();

void epaper_task(void *arg);
