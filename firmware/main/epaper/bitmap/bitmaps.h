#pragma once

#include <stdint.h>

typedef struct {
	const uint8_t *table;
	uint16_t width;
	uint16_t height;
} bitmap_t;

extern const bitmap_t UMICH;
