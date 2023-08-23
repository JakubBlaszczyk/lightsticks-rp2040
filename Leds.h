#ifndef _LEDS_
#define _LEDS_

#include "Common.h"
#include <stdint.h>

typedef struct
{
  uint8_t LedCount;
  COLOR_GRB Color;
  uint8_t TemperatureIndex;
} LED_SECTION;

#define TEMPERATURE_UNCORRECTED 0
#define TEMPERATURE_CANDLE 1
#define TEMPERATURE_HALOGEN 2
#define TEMPERATURE_COOLWHITEFLUORESCENT 3
#define TEMPERATURE_FULLSPECTRUMFLUORESCENT 4

#define MINIMAL_BRIGHTNESS 10
#define RAND_THRESHOLD (RAND_MAX - 30)

void InitializeConfigs(uint8_t AmountOfConfigs);
uint8_t InitializeConfig(uint8_t ConfigIndex, uint8_t AmountOfSections, const uint8_t *LedCounts, uint8_t *Buffer, uint16_t BufferSize);
LED_SECTION *GetLedSection(const uint8_t ConfigIndex, const uint8_t SectionIndex);
uint8_t FillHalfBuffer(const uint8_t ConfigIndex);
uint8_t PrepareBufferForTransaction(const uint8_t ConfigIndex);
void TurnOffLeds(uint8_t ConfigIndex);
void ShowEffectRainbow(uint8_t ConfigIndex, uint8_t ColorStep, uint8_t HueStep);
void ShowEffectBrightness(uint8_t ConfigIndex, uint8_t Brightness);
void ShowEffectFade(uint8_t ConfigIndex, uint8_t Step);
COLOR_GRB ShowEffectPaletteSmoothTransition(uint8_t ConfigIndex, uint8_t HueStep, PALETTE_ARRAY *Pallete);
void ShowEffectPaletteInstantTransition(uint8_t ConfigIndex, uint8_t HueStep, PALETTE_ARRAY *Pallete);
void ShowEffectGlitter(uint8_t ConfigIndex);

#endif // _LEDS_
