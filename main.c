#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/watchdog.h>
#include <hardware/pio.h>
#include <hardware/irq.h>
#include <hardware/dma.h>
#include <hardware/xosc.h>
#include <hardware/clocks.h>
#include <hardware/structs/scb.h>
#include <hardware/timer.h>
#include <hardware/sync.h>
#include "ws2812.pio.h"
#include "Common.h"
#include "Leds.h"
#include "Effect.h"
#include "rp2040_specific.h"

#define LED_AMOUNT 18
// #define LED_AMOUNT 1
#define BUTTON_PRESSED 0
#define BUTTON_NOT_PRESSED 1
#define MAX_BRIGHTNESS 130
#define BRIGHTNESS_STEP 10

static const COLOR_GRB Miku = {212, 1, 59};
static const COLOR_GRB Kaito = {9, 34, 255};
static const COLOR_GRB Yellow = {229, 216, 0};
static const COLOR_GRB Orange = {150, 255, 0};
static const COLOR_GRB Red = {4, 189, 4};
static const COLOR_GRB Luka = {105, 255, 180};
static const COLOR_GRB Gumi = {255, 25, 25};
static const COLOR_GRB White = {255, 255, 255};
static const COLOR_GRB Ai = {0, 75, 130};
static PALETTE gMikuSolidPalette[] = {{0, Miku}};
static PALETTE gKaitoSolidPalette[] = {{0, Kaito}};
static PALETTE gYellowSolidPalette[] = {{0, Yellow}};
static PALETTE gOrangeSolidPalette[] = {{0, Orange}};
static PALETTE gRedSolidPalette[] = {{0, Red}};
static PALETTE gLukaSolidPalette[] = {{0, Luka}};
static PALETTE gGumiSolidPalette[] = {{0, Gumi}};
static PALETTE gWhiteSolidPalette[] = {{0, White}};
static PALETTE gAiSolidPalette[] = {{0, Ai}};
static PALETTE gRedTransitivePalette[] = {{0, Red}, {50, Miku}, {170, Kaito}, {255, Red}};
static PALETTE gGreenTransitivePalette[] = {{0, Ai}, {128, White}, {255, Ai}};
static PALETTE gBlueTransitivePalette[] = {{0, Miku}, {60, Miku}, {128, Luka}, {180, Luka}, {255, Miku}};
static PALETTE_ARRAY gTransitivePalettes[] = {
  {gGreenTransitivePalette, LENGTH_OF (gGreenTransitivePalette)},
  {gBlueTransitivePalette, LENGTH_OF (gBlueTransitivePalette)},
  {gRedTransitivePalette, LENGTH_OF (gRedTransitivePalette)}};
static PALETTE_ARRAY gSolidPalettes[] = {
                                        {gMikuSolidPalette, 1},
                                        {gKaitoSolidPalette, 1},
                                        {gYellowSolidPalette , 1},
                                        {gOrangeSolidPalette, 1},
                                        {gRedSolidPalette, 1},
                                        {gLukaSolidPalette, 1},
                                        {gGumiSolidPalette, 1},
                                        {gWhiteSolidPalette, 1},
                                        {gAiSolidPalette, 1}
                                        };
static uint8_t gEffectsIndex = 0;
static uint8_t gPaletteIndex = 0;
static uint8_t gBrightness = 8;
static uint8_t gTurnOff = 0;
static bool    gChanging = false;
static int8_t  gDecoderValue = 0;

static void ShowEffectRainbowWrapper(void) {
  ShowEffectRainbow(0, 6, 2);
  gChanging = true;
}

static void ShowEffectPaletteSmoothTransitionWrapper(void) {
  ShowEffectPaletteSmoothTransition(0, 1, &gTransitivePalettes[gPaletteIndex % LENGTH_OF (gTransitivePalettes)]);
  gChanging = true;
}

static void ShowEffectPaletteInstantTransitionWrapper(void) {
  ShowEffectPaletteInstantTransition(0, 2, &gSolidPalettes[gPaletteIndex % LENGTH_OF (gSolidPalettes)]);
  gChanging = false;
}

static void ShowEffectBrightnessWrapper(void) {
  ShowEffectBrightness(0, ((gBrightness * BRIGHTNESS_STEP) % MAX_BRIGHTNESS) + MINIMAL_BRIGHTNESS);
  gChanging = false;
} 

static void (*gEffects[])(void) = {ShowEffectPaletteInstantTransitionWrapper, ShowEffectBrightnessWrapper};
static const uint8_t gEffectsSize = LENGTH_OF(gEffects);

static void UpdatePaletteIndex(uint8_t Change) {
  if (gEffects[gEffectsIndex] == ShowEffectBrightnessWrapper) {
    gBrightness += Change;
  } else {
    gPaletteIndex += Change;
  }
}

static void UpdateEffectsIndex() {
  gEffectsIndex++;
}

static void EncoderGpioCallback(uint gpio, uint32_t event_mask) {
  uint8_t PinCurrentState = gpio_get(gpio);
  if (gpio == PRIV_DECODER_LEFT_GPIO_PIN) {
    gDecoderValue -= PinCurrentState > 0;
  } else if (gpio == PRIV_DECODER_RIGHT_GPIO_PIN) {
    gDecoderValue += PinCurrentState > 0;
  }
}

static bool UpdateEncoderLogic() {
  static int8_t PreviousDecoderValue = 0;
  int8_t Difference = gDecoderValue - PreviousDecoderValue;

  bool Change = false;
  if (Difference != 0) {
      UpdateEffectsIndex (Change);
      Change = true;
  }

  PreviousDecoderValue = gDecoderValue;
  return Change;
}

static bool UpdateButtonLogic(uint16_t GpioPin) {
  static uint8_t PreviousPinState = 0;
  static uint16_t PinHoldTime = 0;

  uint8_t CurrentPinState = gpio_get(GpioPin);
  bool Change = false;
  if (CurrentPinState != PreviousPinState) {
    if (PreviousPinState == BUTTON_PRESSED) {
      if (PinHoldTime < RESET_HOLD_TIME && PinHoldTime >= MINIMAL_HOLD_TIME) {
        UpdateEffectsIndex();
        Change = true;
      }
    } else if (PreviousPinState == BUTTON_NOT_PRESSED) {
      if (PinHoldTime >= RESET_HOLD_TIME) {
        ShutdownProcessor();
      }
      PinHoldTime = 0;
    }
  } else if (CurrentPinState == BUTTON_PRESSED) {
    if (PinHoldTime >= RESET_HOLD_TIME) {
      Change = true;
      gTurnOff = 1;
    }
    PinHoldTime++;
  }

  PreviousPinState = CurrentPinState;
  return Change;
}

static void UpdateLeds() {
  if (gTurnOff) {
    TurnOffLeds(0);
  } else {
    gEffects[gEffectsIndex % gEffectsSize]();
  }
  PrepareBufferForTransaction(0);
}

uint8_t gLedBuffer[24 * (LED_AMOUNT + 4)];
const uint16_t gLedBufferSize = LENGTH_OF(gLedBuffer);

// single click - change color
// single hold - change effect
// single long (1.6s) hold - turn off
static void HwTimerCallback(uint alarm_num) {
  bool Change = UpdateEncoderLogic();
  Change |= UpdateButtonLogic(PRIV_BUTTON_GPIO_PIN);
  printf("Callback\n\r");
  watchdog_update();
  if (Change || gChanging) {
    for (int i = 0; i < CLK_COUNT; i++) {
      printf ("Clock %d:%d\n\r", i, clock_get_hz(i));
    }
    printf ("PaletteIndex %d\n\r", gPaletteIndex);
    UpdateLeds();
    StartDmaTransfer(gLedBuffer);
    printf("Change\n\r");
  }
  SetTimerInterrupt(HwTimerCallback);
}

int main() {
  clocks_init();
  LimitClocks();
  InitializeButton();
  InitializeEncoder(EncoderGpioCallback);
  watchdog_enable(WDG_TIMEOUT_MS, false);

  PIO Pio = pio0;
  int Sm = 0;
  uint Offset = pio_add_program(Pio, &ws2812_program);
  ws2812_program_init(Pio, Sm, Offset, PRIV_LEDS_GPIO_PIN, 800000, false);
  DmaInit(gLedBuffer, 24 * (LED_AMOUNT + 4), Pio, Sm);

  InitializeConfigs(1);
  InitializeConfig(0, LED_AMOUNT, NULL, gLedBuffer, gLedBufferSize);
  for (uint8_t Index = 0; Index < LED_AMOUNT; Index++) {
    GetLedSection(0, Index)->Color = gSolidPalettes[0].Palette[0].Grb;
  }

  PrepareBufferForTransaction(0);
  StartDmaTransfer(gLedBuffer);
  sleep_ms(WDG_TIMEOUT_MS / 2);
  SetTimerInterrupt(HwTimerCallback);
  GoToSleep();
  while (true) {
    printf("Should not be here");
    sleep_ms(WDG_TIMEOUT_MS / 2);
  }
}
