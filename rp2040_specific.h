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

#define DMA_CHANNEL 0
#define DMA_CHANNEL_MASK (1u << DMA_CHANNEL)
// #define PRIV_LEDS_GPIO_PIN 23
// #define PRIV_USER_GPIO_PIN 24
#define PRIV_DECODER_LEFT_GPIO_PIN 2
#define PRIV_DECODER_RIGHT_GPIO_PIN 2
#define PRIV_LEDS_GPIO_PIN 2
#define PRIV_BUTTON_GPIO_PIN 1
#define SHUTDOWN_ALL_MEMORIES_BITS 0xF

static void GoToSleep() {
  // only watchdog and timers have to work
  clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS | CLOCKS_SLEEP_EN1_CLK_SYS_WATCHDOG_BITS;

  uint save = scb_hw->scr;
  // Enable deep sleep and sleep on exit
  scb_hw->scr = save | M0PLUS_SCR_SLEEPDEEP_BITS | M0PLUS_SCR_SLEEPONEXIT_BITS;
  __wfi();
}

static void InitializeButton() {
  gpio_init(PRIV_BUTTON_GPIO_PIN);
  gpio_set_dir(PRIV_BUTTON_GPIO_PIN, GPIO_IN);
  gpio_pull_up(PRIV_BUTTON_GPIO_PIN);
}

static void InitializeEncoder(gpio_irq_callback_t gpio_callback) {
  gpio_init(PRIV_DECODER_LEFT_GPIO_PIN);
  gpio_set_dir(PRIV_DECODER_LEFT_GPIO_PIN, GPIO_IN);
  gpio_pull_up(PRIV_DECODER_LEFT_GPIO_PIN);
  gpio_set_irq_enabled_with_callback(PRIV_DECODER_LEFT_GPIO_PIN, GPIO_IRQ_EDGE_RISE, true, gpio_callback);

  gpio_init(PRIV_DECODER_RIGHT_GPIO_PIN);
  gpio_set_dir(PRIV_DECODER_RIGHT_GPIO_PIN, GPIO_IN);
  gpio_pull_up(PRIV_DECODER_RIGHT_GPIO_PIN);
  gpio_set_irq_enabled_with_callback(PRIV_DECODER_RIGHT_GPIO_PIN, GPIO_IRQ_EDGE_RISE, true, gpio_callback);
}

static void LimitClocks() {
  clock_configure(clk_sys,
                  CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                  CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                  SYS_CLK_KHZ * KHZ,
                  (SYS_CLK_KHZ / 10) * KHZ);
  clock_stop(clk_rtc);
  clock_stop(clk_adc);
  clock_stop(clk_usb);
  clock_stop(clk_peri);
  clock_stop(clk_gpout0);
  clock_stop(clk_gpout1);
  clock_stop(clk_gpout2);
  clock_stop(clk_gpout3);
}

static void ShutdownProcessor() {
  gpio_set_dormant_irq_enabled(PRIV_BUTTON_GPIO_PIN, IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_EDGE_HIGH_BITS, true);
  *(uint32_t*)(SYSCFG_BASE) = SHUTDOWN_ALL_MEMORIES_BITS;
  xosc_dormant();
  gpio_acknowledge_irq(PRIV_BUTTON_GPIO_PIN, IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_EDGE_HIGH_BITS);
  watchdog_reboot(0, 0, WDG_TIMEOUT_MS);
}

static void __isr DmaCompleteHandler() {
    if (dma_hw->ints0 & DMA_CHANNEL_MASK) {
        dma_hw->ints0 = DMA_CHANNEL_MASK;
    }
}

static void DmaInit(uint8_t *buffer_pointer, uint16_t buffer_size, PIO pio, uint sm) {
    dma_claim_mask(DMA_CHANNEL_MASK);

    dma_channel_config channel_config = dma_channel_get_default_config(DMA_CHANNEL);
    channel_config_set_dreq(&channel_config, pio_get_dreq(pio, sm, true));
    channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_8);
    dma_channel_configure(DMA_CHANNEL,
                          &channel_config,
                          &pio->txf[sm],
                          &buffer_pointer[0],
                          buffer_size,
                          false);

  irq_set_exclusive_handler(DMA_IRQ_0, DmaCompleteHandler);
  dma_channel_set_irq0_enabled(DMA_CHANNEL, true);
  irq_set_enabled(DMA_IRQ_0, true);
}

static void StartDmaTransfer(uint8_t *buffer_pointer) {
  if (!dma_channel_is_busy(DMA_CHANNEL)) {
    dma_channel_set_read_addr(DMA_CHANNEL, buffer_pointer, true);
  } else { 
    printf("Channel busy\n\r");
  }
}

static void SetTimerInterrupt(hardware_alarm_callback_t callback) {
  static uint hw_timer_num = -1;
  if (hw_timer_num == -1) {
    hw_timer_num = hardware_alarm_claim_unused(true);
    hardware_alarm_set_callback(hw_timer_num, callback);
  }

  absolute_time_t time;
  update_us_since_boot(&time, time_us_64() + TIMER_DELAY_US);
  hardware_alarm_set_target(hw_timer_num, time);
}

