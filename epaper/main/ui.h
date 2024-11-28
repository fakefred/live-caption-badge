#pragma once

#include "epaper.h"

/*
 * Switches layout to badge mode. Displays logos, name, etc.
 */
epaper_err_t ui_layout_badge(void);

/*
 * Switches layout to caption mode. Displays blank area for caption.
 */
epaper_err_t ui_layout_caption(void);
