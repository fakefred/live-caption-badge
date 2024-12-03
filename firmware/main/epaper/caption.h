#pragma once

#include "epaper.h"

/*
 * Initializes data structures needed for caption.
 * Does not talk to hardware.
 */
epaper_err_t caption_init(caption_cfg_t *cfg);

/*
 * Clears caption area on screen, so that the next chunk of text will be printed in the top left
 * corner.
 */
epaper_err_t caption_clear();

/*
 * Schedules `string` to be printed in caption area on screen.
 *
 * string: null-terminated C string
 */
epaper_err_t caption_append(const char *string);

/*
 * Updates area on screen dedicated to caption.
 */
epaper_err_t caption_display();
