#include <stdint.h>

#include "Common.h"
#include "Effect.h"

COLOR_GRB GetColorFromPalleteSmooth (uint8_t Angle, PALLETE_ARRAY *PalleteArray) {
  uint8_t Index;
  uint8_t Found = 0;
  PALLETE *Pallete = PalleteArray->Pallete;
  for (Index = 0; Index < PalleteArray->Length - 1; Index++) {
    if (Angle >= Pallete[Index].Angle && Angle < Pallete[Index + 1].Angle) {
      Found = 1;
      break;
    }
  }

  if (Found) {
    COLOR_HSV First = RgbToHsv(Pallete[Index].Rgb);
    COLOR_HSV Second = RgbToHsv(Pallete[Index + 1].Rgb);
    uint8_t a = First.h;
    uint8_t b = Second.h;
    float t = (((float)Angle - (float)Pallete[Index].Angle) * (255.0 / ((float)Pallete[Index + 1].Angle - (float)Pallete[Index].Angle)) / 255.0);
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
    return Pallete[Index].Rgb;
  }
}

COLOR_GRB GetColorFromPalleteSolid (uint8_t Angle, PALLETE_ARRAY *PalleteArray) {
  uint8_t Index;
  PALLETE *Pallete = PalleteArray->Pallete;
  for (Index = 0; Index < PalleteArray->Length - 1; Index++) {
    if (Angle >= Pallete[Index].Angle && Angle < Pallete[Index + 1].Angle) {
      break;
    }
  }

  return Pallete[Index].Rgb;
}

uint8_t LerpHSV (uint8_t a, uint8_t b, float t) {
  float tempa = (float)(a) / 255;
  float tempb = (float)(b) / 255;

  float tempd = (float)(b - a) / 255;
  float h;
  uint8_t hue;

  if (tempa > tempb) {
    float temp = tempa;
    tempa = tempb;
    tempb = temp;
    tempd = -tempd;
    t = 1 - t;
  }

  if (tempd > 0.5) {
    tempa = tempa + 1;
    h = (tempa + t * (tempb - tempa)); // 360deg
    while (h > 1.0) {
      h -= 1.0;
    }
    while (h < -1.0) {
      h += 1.0;
    }
  } else if (tempd <= 0.5) {
    h = tempa + t * tempd;
  }
   hue = h * 255;
  return hue;
}

// #include <stdio.h>

// int main() {
//   for (int i = 0; i < 255; i++) {
//     printf("%d: %d\n", i, LerpHSV(0, 180, i));
//   }
// }