#pragma once

#include <stdint.h>

typedef struct {
	const uint8_t *table;
	uint16_t width;
	uint16_t height;
} bitmap_t;

extern const bitmap_t UMICH_LOGO;
extern const bitmap_t WIFI_LOGO;
extern const bitmap_t NO_WIFI_LOGO;
extern const bitmap_t MUTE_LOGO;
extern const bitmap_t UNMUTE_LOGO;
extern const bitmap_t PAIR_LOGO;
extern const bitmap_t CHECK_LOGO;
extern const bitmap_t CROSS_LOGO;
extern const bitmap_t LINK_LOGO;
extern const bitmap_t UNLINK_LOGO;
