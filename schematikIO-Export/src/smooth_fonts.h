#pragma once
#include <Arduino.h>

// Font data structure for antialiased fonts
struct SmoothGlyph { uint32_t offset; uint16_t advance; uint8_t w,h; int8_t x,y; };
struct SmoothFont { const uint8_t* bits; const SmoothGlyph* glyphs; int cap; };

#include "system_fonts.h"

extern uint8_t activeFontId;
