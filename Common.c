#include "Common.h"

COLOR_GRB HsvToRgb(COLOR_HSV hsv) {
  COLOR_GRB rgb;
  uint8_t region, remainder, p, q, t;

  if (hsv.s == 0) {
      rgb.Red = hsv.v;
      rgb.Green = hsv.v;
      rgb.Blue = hsv.v;
      return rgb;
  }

  region = hsv.h / 43;
  remainder = (hsv.h - (region * 43)) * 6;

  p = (hsv.v * (255 - hsv.s)) >> 8;
  q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
  t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
      case 0:
          rgb.Red = hsv.v; rgb.Green = t; rgb.Blue = p;
          break;
      case 1:
          rgb.Red = q; rgb.Green = hsv.v; rgb.Blue = p;
          break;
      case 2:
          rgb.Red = p; rgb.Green = hsv.v; rgb.Blue = t;
          break;
      case 3:
          rgb.Red = p; rgb.Green = q; rgb.Blue = hsv.v;
          break;
      case 4:
          rgb.Red = t; rgb.Green = p; rgb.Blue = hsv.v;
          break;
      default:
          rgb.Red = hsv.v; rgb.Green = p; rgb.Blue = q;
          break;
  }

  return rgb;
}

COLOR_HSV RgbToHsv(COLOR_GRB rgb) {
  COLOR_HSV hsv;
  uint8_t rgbMin, rgbMax;

  rgbMin = rgb.Red < rgb.Green ? (rgb.Red < rgb.Blue ? rgb.Red : rgb.Blue) : (rgb.Green < rgb.Blue ? rgb.Green : rgb.Blue);
  rgbMax = rgb.Red > rgb.Green ? (rgb.Red > rgb.Blue ? rgb.Red : rgb.Blue) : (rgb.Green > rgb.Blue ? rgb.Green : rgb.Blue);

  hsv.v = rgbMax;
  if (hsv.v == 0) {
      hsv.h = 0;
      hsv.s = 0;
      return hsv;
  }

  hsv.s = 255 * (int32_t)(rgbMax - rgbMin) / hsv.v;
  if (hsv.s == 0) {
      hsv.h = 0;
      return hsv;
  }

  if (rgbMax == rgb.Red) {
    hsv.h = 0 + 43 * (rgb.Green - rgb.Blue) / (rgbMax - rgbMin);
  } else if (rgbMax == rgb.Green) {
    hsv.h = 85 + 43 * (rgb.Blue - rgb.Red) / (rgbMax - rgbMin);
  } else {
    hsv.h = 171 + 43 * (rgb.Red - rgb.Green) / (rgbMax - rgbMin);
  }

  return hsv;
}
