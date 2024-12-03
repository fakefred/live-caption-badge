#pragma once

/*
 * The epaper is a daemon running in its own task (epaper_task). To use the epaper:
 *
 * 1. Invoke epaper_init()
 * 2. Control epaper layout and contents with epaper_ui_* functions
 * 3. When done, invoke epaper_shutdown()
 *
 * This implementation is not concurrent. Only ONE task is allowed to invoke epaper_*
 * functions.
 */

#include "DEV_Config.h"
#include "font/fonts.h"
#include <stdint.h>

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

typedef enum {
	EPAPER_OK,
	EPAPER_ERR,
} epaper_err_t;

typedef enum {
	EPAPER_REFRESH_SLOW,
	EPAPER_REFRESH_FAST,
	EPAPER_REFRESH_PARTIAL,
	EPAPER_REFRESH_CLEAR,
	EPAPER_REFRESH_SLEEP,
} epaper_refresh_mode_t;

typedef struct {
	epaper_refresh_mode_t mode;
	// bounding box position, in pixels
	// start-inclusive, end-exclusive
	UWORD x_start, y_start, x_end, y_end;
} epaper_refresh_area_t;

extern QueueHandle_t epaper_refresh_queue; // queue of areas to refresh

typedef enum {
	EPAPER_LAYOUT_BADGE,
	EPAPER_LAYOUT_PAIR,
	EPAPER_LAYOUT_CAPTION,
} epaper_layout_t;

typedef struct {
	epaper_layout_t layout;
} epaper_ui_t;

extern epaper_ui_t epaper_ui;

typedef struct {
	// bounding box position, in pixels
	UWORD x_start, y_start, x_end, y_end;
	sFONT *font;
} caption_cfg_t;

/*
 * Initializes epaper and FreeRTOS stuff. Starts epaper_task.
 */
epaper_err_t epaper_init(void);

/*
 * Sets UI layout.
 *
 * The epaper can be in one of the following layouts: badge, pair, caption.
 */
epaper_err_t epaper_ui_set_layout(epaper_layout_t layout);

/*
 * Clears epaper and puts epaper to sleep. Stops epaper_task.
 */
epaper_err_t epaper_shutdown(void);
