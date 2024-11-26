#pragma once

#include "DEV_Config.h"
#include "fonts.h"
#include "freertos/idf_additions.h"
#include <stdint.h>

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

extern MessageBufferHandle_t caption_buf;

typedef enum {
	EPAPER_OK,
	EPAPER_ERR,
} epaper_err_t;

typedef enum {
	EPAPER_STATE_UNINITIALIZED,
	EPAPER_STATE_IDLE,
	EPAPER_STATE_CAPTION,
	EPAPER_STATE_SHUTDOWN,
} epaper_state_t;

extern epaper_state_t epaper_state;

typedef struct {
	// bounding box position, in pixels
	UWORD x_start, y_start, x_end, y_end;
	sFONT *font;
} caption_cfg_t;

epaper_err_t epaper_init(void);
epaper_err_t epaper_set_state(epaper_state_t new_state);
epaper_err_t epaper_shutdown(void);

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
