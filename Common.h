#ifndef _COMMON_
#define _COMMON_

#include <stdint.h>

#define LENGTH_OF(Array) (sizeof(Array)/sizeof(Array[0]))

typedef struct
{
  uint8_t Green;
  uint8_t Red;
  uint8_t Blue;
} COLOR_GRB;

typedef struct
{
  uint8_t h;
  uint8_t s;
  uint8_t v;
} COLOR_HSV;

typedef struct {
  uint8_t Angle;
  COLOR_GRB Rgb;
} PALLETE;

typedef struct {
  PALLETE *Pallete;
  uint8_t Length;
} PALLETE_ARRAY;

COLOR_GRB HsvToRgb(COLOR_HSV hsv);
COLOR_HSV RgbToHsv(COLOR_GRB rgb);

#endif // _COMMON_
