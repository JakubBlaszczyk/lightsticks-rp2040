#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/watchdog.h>
#include <hardware/uart.h>
#include <hardware/pwm.h>
#include <hardware/pio.h>
#include <hardware/irq.h>
#include <hardware/dma.h>
#include "ws2812.pio.h"

#define PRIV_WATCHDOG_TIMEOUT 1000u

#define PRESCALER 1
#define COUNTER 155
#define HIGH_LEVEL 60
#define LOW_LEVEL 30

// #define PRIV_UART_BAUD_RATE 115200
// #define PRIV_UART_ID uart0
// #define PRIV_UART_TX_PIN 0
// #define PRIV_UART_RX_PIN 1

#define PRIV_LEDS_GPIO_PIN 23
#define PRIV_USER_GPIO_PIN 24

void button_callback(uint gpio, uint32_t events) {
  printf("GPIO %d, events %d\n", gpio, events);
}

static void initialize_button(uint8_t pin) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  gpio_pull_up(pin);
  gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_RISE, true, &button_callback);
}

#define FRAC_BITS 4
#define NUM_PIXELS 64

// static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
//     return
//             ((uint32_t) (r) << 8) |
//             ((uint32_t) (g) << 16) |
//             (uint32_t) (b);
// }

// void pattern_random(uint len, uint t) {
//     if (t % 8)
//         return;
//     for (int i = 0; i < len; ++i)
//         put_pixel(rand());
// }

#define DMA_CHANNEL 0
#define DMA_CHANNEL_MASK (1u << DMA_CHANNEL)

void __isr dma_complete_handler() {
    if (dma_hw->ints0 & DMA_CHANNEL_MASK) {
        dma_hw->ints0 = DMA_CHANNEL_MASK;
        printf("Dma transfer complete\n\r");
    }
}

void dma_init(uint8_t *buffer_pointer, PIO pio, uint sm) {
    dma_claim_mask(DMA_CHANNEL_MASK);

    dma_channel_config channel_config = dma_channel_get_default_config(DMA_CHANNEL);
    channel_config_set_dreq(&channel_config, pio_get_dreq(pio, sm, true));
    // channel_config_set_irq_quiet(&channel_config, true);
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

void output_strips_dma(uint8_t *buffer_pointer) {
    // dma_channel_config channel_config = dma_get_channel_config(DMA_CHANNEL);
    // dma_get_channel_config(DMA_CHANNEL);
    dma_channel_set_read_addr(DMA_CHANNEL, buffer_pointer, true);
}

int main() {
  stdio_init_all();
  watchdog_enable(PRIV_WATCHDOG_TIMEOUT, true);
  initialize_button(PRIV_USER_GPIO_PIN);

  uint8_t buffer[1024];
  PIO pio = pio0;
  int sm = 0;
  uint offset = pio_add_program(pio, &ws2812_program);

  ws2812_program_init(pio, sm, offset, PRIV_LEDS_GPIO_PIN, 800000, false);

  dma_init(buffer, pio, sm);
  int t = 0;
  uint8_t value = 0;
  while (1) {
    for (int i = 0; i < 32; i++) {
      buffer[i] = ((value >> (i >> (7 - (i % 8))) & 0x1);
      printf("%d:", buffer[i]);
    }
    printf("\b\n\r");
    for (int i = 32; i < 1024; i++) {
      buffer[i] = 0;
    }

    watchdog_update();
    // pattern_random(NUM_PIXELS, t);
    output_strips_dma(buffer);
    value = (value + 22) % 122;
    dma_channel_wait_for_finish_blocking(DMA_CHANNEL);
    printf("Value %d\n\r", value);
    sleep_ms(500);
  }
  // while (true) {
    // for (int i = 0; i < 30; i++) {
    //   printf("%d GPIO %d\n", i, gpio_get(i));
    // }

    // printf("Initialized\n");
    // if (counter == 0) {
    //   pwm_set_chan_level(slice, PWM_CHAN_A, HIGH_LEVEL);
    //   counter++;
    // } else if (counter == 1) {
    //   pwm_set_chan_level(slice, PWM_CHAN_A, LOW_LEVEL);
    //   counter++;
    // } else {
    //   pwm_set_chan_level(slice, PWM_CHAN_A, 0);
    //   counter = 0;
    // }
    // watchdog_update();
    // printf("Sending by UART\n");
  //   sleep_ms(500);
  // }
}
