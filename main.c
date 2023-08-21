#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/watchdog.h>
#include <hardware/uart.h>
#include <hardware/pwm.h>
#include <hardware/pio.h>
#include <hardware/irq.h>
#include <hardware/dma.h>
#include <hardware/xosc.h>
#include <hardware/clocks.h>
#include <hardware/rtc.h>
#include <hardware/structs/scb.h>
#include <hardware/timer.h>
#include <hardware/sync.h>
#include "ws2812.pio.h"
#include "Common.h"
#include "Leds.h"
#include "Effect.h"

// #define LED_AMOUNT 18
#define LED_AMOUNT 1
#define GPIO_PIN_INDEX_LEFT 0
#define BUTTON_PRESSED 0
#define BUTTON_NOT_PRESSED 1
#define IWDG_TIMEOUT 1638 // ms
#define RESET_HOLD_TIME (uint8_t)(IWDG_TIMEOUT / 16.65)
#define MINIMAL_HOLD_TIME (uint8_t)(400 / 16.65)
#define MINIMAL_CLICK_TIME (uint8_t)(40 / 16.65)


#define PRIV_LEDS_GPIO_PIN 23
#define PRIV_USER_GPIO_PIN 24
// #define PRIV_LEDS_GPIO_PIN 8
// #define PRIV_USER_GPIO_PIN 7

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
  ShowEffectBrightness(0, ((gPalleteIndex * 10) % 50) + MINIMAL_BRIGHTNESS);
  gChanging = false;
} 

static void (*gEffects[])(void) = {ShowEffectPalleteInstantTransitionWrapper, ShowEffectBrightnessWrapper};
static const uint8_t gEffectsSize = LENGTH_OF(gEffects);

static void UpdatePalleteIndex() {
  gPalleteIndex++;
}

static void UpdateEffectsIndex() {
  gEffectsIndex++;
}

#define SHUTDOWN_ALL_MEMORIES_BITS 0xF

static void shutdown_memory() {
  *(uint32_t*)(SYSCFG_BASE) = SHUTDOWN_ALL_MEMORIES_BITS;
}

static void shutdown_processor() {
  gpio_set_dormant_irq_enabled(PRIV_USER_GPIO_PIN, IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_EDGE_HIGH_BITS, true);
  shutdown_memory();
  xosc_dormant();
  gpio_acknowledge_irq(PRIV_USER_GPIO_PIN, IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_EDGE_HIGH_BITS);
  watchdog_reboot(0, 0, IWDG_TIMEOUT);
}

static void private_sleep() {
  // only watchdog and timers have to work
  clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS | CLOCKS_SLEEP_EN1_CLK_SYS_WATCHDOG_BITS;

  uint save = scb_hw->scr;
  // Enable deep sleep and sleep on exit
  scb_hw->scr = save | M0PLUS_SCR_SLEEPDEEP_BITS | M0PLUS_SCR_SLEEPONEXIT_BITS;

  // Go to sleep
  __wfi();
}

static bool UpdatePinLogic(uint16_t GpioPin) {
  uint8_t GpioPinCurrentState = gpio_get(GpioPin);
  bool Change = false;
  if (GpioPinCurrentState != gPinState) {
    if (gPinState == BUTTON_PRESSED) {
      if (gPinHoldTime < RESET_HOLD_TIME && gPinHoldTime >= MINIMAL_HOLD_TIME) {
        UpdateEffectsIndex();
        Change = true;
        // printf("Effect\n\r");
      } else if (gPinHoldTime < MINIMAL_HOLD_TIME && gPinHoldTime >= MINIMAL_CLICK_TIME) {
        UpdatePalleteIndex();
        Change = true;
        // printf("Pallete\n\r");
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

#define PRIV_WATCHDOG_TIMEOUT 1000u

// #define PRIV_UART_BAUD_RATE 115200
// #define PRIV_UART_ID uart0
// #define PRIV_UART_TX_PIN 0
// #define PRIV_UART_RX_PIN 1

// void button_callback(uint gpio, uint32_t events) {
//   printf("GPIO %d, events %d\n", gpio, events);
// }

static void initialize_button(uint8_t pin) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  gpio_pull_up(pin);
  // gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_RISE, true, &button_callback);
}

#define DMA_CHANNEL 0
#define DMA_CHANNEL_MASK (1u << DMA_CHANNEL)

void __isr dma_complete_handler() {
    if (dma_hw->ints0 & DMA_CHANNEL_MASK) {
        dma_hw->ints0 = DMA_CHANNEL_MASK;
    }
}

void dma_init(uint8_t *buffer_pointer, PIO pio, uint sm) {
    dma_claim_mask(DMA_CHANNEL_MASK);

    dma_channel_config channel_config = dma_channel_get_default_config(DMA_CHANNEL);
    channel_config_set_dreq(&channel_config, pio_get_dreq(pio, sm, true));
    channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_8);
    dma_channel_configure(DMA_CHANNEL,
                          &channel_config,
                          &pio->txf[sm],
                          &buffer_pointer[0],
                          24,
                          false);

  irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_handler);
  dma_channel_set_irq0_enabled(DMA_CHANNEL, true);
  irq_set_enabled(DMA_IRQ_0, true);
}

void start_dma_transfer(uint8_t *buffer_pointer) {
  if (!dma_channel_is_busy(DMA_CHANNEL)) {
    dma_channel_set_read_addr(DMA_CHANNEL, buffer_pointer, true);
  } else { 
    printf("Channel busy\n\r");
  }
}

uint8_t gLedBuffer[24 * (LED_AMOUNT + 4)];
const uint16_t gLedBufferSize = LENGTH_OF(gLedBuffer);

static void set_timer_interrupt();
static void hw_timer_callback(uint alarm_num);

static void set_timer_interrupt() {
  static uint hw_timer_num = -1;
  if (hw_timer_num == -1) {
    hw_timer_num = hardware_alarm_claim_unused(true);
    hardware_alarm_set_callback(hw_timer_num, hw_timer_callback);
  }

  absolute_time_t time;
  update_us_since_boot(&time, time_us_64() + 16600);
  bool check = hardware_alarm_set_target(hw_timer_num, time);
  printf("check %d\n\r", check);
}

// single click - change color
// single hold - change effect
// single long (1.6s) hold - turn off
static void hw_timer_callback(uint alarm_num) {
  bool Change = UpdatePinLogic(PRIV_USER_GPIO_PIN);
  printf("Callback\n\r");
  watchdog_update();
  if (Change || gChanging) {
    printf ("PalleteIndex %d\n\r", gPalleteIndex);
    UpdateLeds();
    start_dma_transfer(gLedBuffer);
    printf("Change\n\r");
  }
  set_timer_interrupt();
}

static bool timer_callback(repeating_timer_t *t) {
    bool Change = UpdatePinLogic(PRIV_USER_GPIO_PIN);
    printf("Callback\n\r");
    watchdog_update();
    if (Change || gChanging) {
      printf ("PalleteIndex %d\n\r", gPalleteIndex);
      UpdateLeds();
      start_dma_transfer(t->user_data);
      printf("Change\n\r");
    }
    return true;
}

int main() {
  stdio_init_all();
  initialize_button(PRIV_USER_GPIO_PIN);

  PIO pio = pio0;
  int sm = 0;
  uint offset = pio_add_program(pio, &ws2812_program);

  ws2812_program_init(pio, sm, offset, PRIV_LEDS_GPIO_PIN, 800000, false);

  dma_init(gLedBuffer, pio, sm);
  int t = 0;
  uint8_t value = 0;
  // alarm_in_us(16665);
  // repeating_timer_t timer;
  // add_repeating_timer_us(-16665, timer_callback, gLedBuffer, &timer);
  // srand(69);
  InitializeConfigs(1);
  InitializeConfig(0, LED_AMOUNT, NULL, gLedBuffer, gLedBufferSize);
  for (uint8_t Index = 0; Index < LED_AMOUNT; Index++) {
    GetLedSection(0, Index)->Color = gSolidPalletes[0].Pallete[0].Rgb;
  }

  PrepareBufferForTransaction(0);
  start_dma_transfer(gLedBuffer);
  watchdog_enable(PRIV_WATCHDOG_TIMEOUT, false);
  sleep_ms(IWDG_TIMEOUT / 2);
  set_timer_interrupt();
  private_sleep();
  while (true) {
    printf("0x%x 0x%x\n\r", *(uint32_t*)(CLOCKS_BASE + CLOCKS_ENABLED0_OFFSET), *(uint32_t*)(CLOCKS_BASE + CLOCKS_ENABLED1_OFFSET));
    watchdog_update();
    sleep_ms(IWDG_TIMEOUT / 2);
    // printf("Working main\n\r");
  }
}
