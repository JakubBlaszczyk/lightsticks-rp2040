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
#define GPIO_PIN_INDEX_LEFT 0
#define BUTTON_PRESSED 0
#define BUTTON_NOT_PRESSED 1
#define MAX_BRIGHTNESS 130
#define BRIGHTNESS_STEP 10

uint8_t gPinState = 0;
uint16_t gPinHoldTime = 0;
static const COLOR_GRB Miku = {212, 1, 59};
static const COLOR_GRB Kaito = {9, 34, 255};
static const COLOR_GRB Yellow = {229, 216, 0};
static const COLOR_GRB Orange = {150, 255, 0};
static const COLOR_GRB Red = {4, 189, 4};
static const COLOR_GRB Luka = {105, 255, 180};
static const COLOR_GRB Gumi = {255, 25, 25};
static const COLOR_GRB White = {255, 255, 255};
static const COLOR_GRB Ai = {0, 75, 130};
static PALLETE gMikuSolidPallete[] = {{0, Miku}};
static PALLETE gKaitoSolidPallete[] = {{0, Kaito}};
static PALLETE gYellowSolidPallete[] = {{0, Yellow}};
static PALLETE gOrangeSolidPallete[] = {{0, Orange}};
static PALLETE gRedSolidPallete[] = {{0, Red}};
static PALLETE gLukaSolidPallete[] = {{0, Luka}};
static PALLETE gGumiSolidPallete[] = {{0, Gumi}};
static PALLETE gWhiteSolidPallete[] = {{0, White}};
static PALLETE gAiSolidPallete[] = {{0, Ai}};
static PALLETE gRedTransitivePallete[] = {{0, Red}, {50, Miku}, {170, Kaito}, {255, Red}};
static PALLETE gGreenTransitivePallete[] = {{0, Ai}, {128, White}, {255, Ai}};
static PALLETE gBlueTransitivePallete[] = {{0, Miku}, {60, Miku}, {128, Luka}, {180, Luka}, {255, Miku}};
static PALLETE_ARRAY gTransitivePalletes[] = {
  {gGreenTransitivePallete, LENGTH_OF (gGreenTransitivePallete)},
  {gBlueTransitivePallete, LENGTH_OF (gBlueTransitivePallete)},
  {gRedTransitivePallete, LENGTH_OF (gRedTransitivePallete)}};
static PALLETE_ARRAY gSolidPalletes[] = {
                                        {gMikuSolidPallete, 1},
                                        {gKaitoSolidPallete, 1},
                                        {gYellowSolidPallete , 1},
                                        {gOrangeSolidPallete, 1},
                                        {gRedSolidPallete, 1},
                                        {gLukaSolidPallete, 1},
                                        {gGumiSolidPallete, 1},
                                        {gWhiteSolidPallete, 1},
                                        {gAiSolidPallete, 1}
                                        };
static uint8_t gEffectsIndex = 0;
static uint8_t gPalleteIndex = 0;
static uint8_t gBrightness = 8;
static uint8_t gTurnOff = 0;
static bool gChanging = false;

static void ShowEffectRainbowWrapper(void) {
  ShowEffectRainbow(0, 6, 2);
  gChanging = true;
}

static void ShowEffectPalleteSmoothTransitionWrapper(void) {
  ShowEffectPalleteSmoothTransition(0, 1, &gTransitivePalletes[gPalleteIndex % LENGTH_OF (gTransitivePalletes)]);
  gChanging = true;
}

static void ShowEffectPalleteInstantTransitionWrapper(void) {
  ShowEffectPalleteInstantTransition(0, 2, &gSolidPalletes[gPalleteIndex % LENGTH_OF (gSolidPalletes)]);
  gChanging = false;
}

static void ShowEffectBrightnessWrapper(void) {
  ShowEffectBrightness(0, ((gBrightness * BRIGHTNESS_STEP) % MAX_BRIGHTNESS) + MINIMAL_BRIGHTNESS);
  gChanging = false;
} 

static void (*gEffects[])(void) = {ShowEffectPalleteInstantTransitionWrapper, ShowEffectBrightnessWrapper};
static const uint8_t gEffectsSize = LENGTH_OF(gEffects);

static void UpdatePalleteIndex() {
  if (gEffects[gEffectsIndex] == ShowEffectBrightnessWrapper) {
    gBrightness++;
  } else {
    gPalleteIndex++;
  }
}

static void UpdateEffectsIndex() {
  gEffectsIndex++;
}

static bool UpdatePinLogic(uint16_t GpioPin) {
  uint8_t GpioPinCurrentState = gpio_get(GpioPin);
  bool Change = false;
  if (GpioPinCurrentState != gPinState) {
    if (gPinState == BUTTON_PRESSED) {
      if (gPinHoldTime < RESET_HOLD_TIME && gPinHoldTime >= MINIMAL_HOLD_TIME) {
        UpdateEffectsIndex();
        Change = true;
      } else if (gPinHoldTime < MINIMAL_HOLD_TIME && gPinHoldTime >= MINIMAL_CLICK_TIME) {
        UpdatePalleteIndex();
        Change = true;
      }
    } else if (gPinState == BUTTON_NOT_PRESSED) {
      if (gPinHoldTime >= RESET_HOLD_TIME) {
        shutdown_processor();
      }
      gPinHoldTime = 0;
    }
  } else if (GpioPinCurrentState == BUTTON_PRESSED) {
    if (gPinHoldTime >= RESET_HOLD_TIME) {
      Change = true;
      gTurnOff = 1;
    }
    gPinHoldTime++;
  }

  gPinState = GpioPinCurrentState;
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
static void hw_timer_callback(uint alarm_num) {
  bool Change = UpdatePinLogic(PRIV_USER_GPIO_PIN);
  printf("Callback\n\r");
  watchdog_update();
  if (Change || gChanging) {
    for (int i = 0; i < CLK_COUNT; i++) {
      printf ("Clock %d:%d\n\r", i, clock_get_hz(i));
    }
    printf ("PalleteIndex %d\n\r", gPalleteIndex);
    UpdateLeds();
    start_dma_transfer(gLedBuffer);
    printf("Change\n\r");
  }
  set_timer_interrupt(hw_timer_callback);
}

int main() {
  clocks_init();
  limit_clocks();
  initialize_button(PRIV_USER_GPIO_PIN);
  watchdog_enable(WDG_TIMEOUT_MS, false);

  PIO pio = pio0;
  int sm = 0;
  uint offset = pio_add_program(pio, &ws2812_program);
  ws2812_program_init(pio, sm, offset, PRIV_LEDS_GPIO_PIN, 800000, false);
  dma_init(gLedBuffer, 24 * (LED_AMOUNT + 4), pio, sm);

  InitializeConfigs(1);
  InitializeConfig(0, LED_AMOUNT, NULL, gLedBuffer, gLedBufferSize);
  for (uint8_t Index = 0; Index < LED_AMOUNT; Index++) {
    GetLedSection(0, Index)->Color = gSolidPalletes[0].Pallete[0].Rgb;
  }

  PrepareBufferForTransaction(0);
  start_dma_transfer(gLedBuffer);
  sleep_ms(WDG_TIMEOUT_MS / 2);
  set_timer_interrupt(hw_timer_callback);
  private_sleep();
  while (true) {
    printf("Should not be here");
    watchdog_update();
    sleep_ms(WDG_TIMEOUT_MS / 2);
  }
}
