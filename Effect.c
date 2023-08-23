#include <stdint.h>

#include "Common.h"
#include "Effect.h"

COLOR_GRB GetColorFromPaletteSmooth (uint8_t Angle, PALETTE_ARRAY *PaletteArray) {
  uint8_t Index;
  uint8_t Found = 0;
  PALETTE *Palette = PaletteArray->Palette;
  for (Index = 0; Index < PaletteArray->Length - 1; Index++) {
    if (Angle >= Palette[Index].Angle && Angle < Palette[Index + 1].Angle) {
      Found = 1;
      break;
    }
  }

  if (Found) {
    COLOR_HSV First = RgbToHsv(Palette[Index].Grb);
    COLOR_HSV Second = RgbToHsv(Palette[Index + 1].Grb);
    uint8_t a = First.h;
    uint8_t b = Second.h;
    float t = (((float)Angle - (float)Palette[Index].Angle) * (255.0 / ((float)Palette[Index + 1].Angle - (float)Palette[Index].Angle)) / 255.0);
    COLOR_HSV Hsv;
    Hsv.h = LerpHSV (a, b, t);
    a = First.s;
    b = Second.s;
    Hsv.s = LerpHSV (a, b, t);
    a = First.v;
    b = Second.v;
    Hsv.v = LerpHSV (a, b, t);
    return HsvToRgb(Hsv);
  } else {
    return Palette[Index].Grb;
  }
}

COLOR_GRB GetColorFromPaletteSolid (uint8_t Angle, PALETTE_ARRAY *PaletteArray) {
  uint8_t Index;
  PALETTE *Palette = PaletteArray->Palette;
  for (Index = 0; Index < PaletteArray->Length - 1; Index++) {
    if (Angle >= Palette[Index].Angle && Angle < Palette[Index + 1].Angle) {
      break;
    }
  }

  return Palette[Index].Grb;
}

uint8_t LerpHSV (uint8_t a, uint8_t b, float t) {
  float FloatA = (float)(a) / 255;
  float FloatB = (float)(b) / 255;

  float FloatD = (float)(b - a) / 255;
  float H;
  uint8_t Hue;

  if (FloatA > FloatB) {
    float temp = FloatA;
    FloatA = FloatB;
    FloatB = temp;
    FloatD = -FloatD;
    t = 1 - t;
  }

  if (FloatD > 0.5) {
    FloatA = FloatA + 1;
    H = (FloatA + t * (FloatB - FloatA)); // 360deg
    while (H > 1.0) {
      H -= 1.0;
    }
    while (H < -1.0) {
      H += 1.0;
    }
  } else if (FloatD <= 0.5) {
    H = FloatA + t * FloatD;
  }
   Hue = H * 255;
  return Hue;
}
