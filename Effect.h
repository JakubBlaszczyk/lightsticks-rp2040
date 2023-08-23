
#ifndef _EFFECT_
#define _EFFECT_

#include "Common.h"

COLOR_GRB GetColorFromPaletteSmooth (uint8_t Angle, PALETTE_ARRAY *PalleteArray);
COLOR_GRB GetColorFromPaletteSolid (uint8_t Angle, PALETTE_ARRAY *PalleteArray);
uint8_t LerpHSV (uint8_t a, uint8_t b, float t);


#endif // _EFFECT_
