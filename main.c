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

void gpio_callback(uint gpio, uint32_t events) {
  printf("GPIO %d, events %d\n", gpio, events);
}

static void initialize_gpio(uint8_t pin) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  gpio_pull_up(pin);
  gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
}

// bit plane content dma channel
#define DMA_CHANNEL 0
// chain channel for configuring main dma channel to output from disjoint 8 word fragments of memory
#define DMA_CB_CHANNEL 1

#define DMA_CHANNEL_MASK (1u << DMA_CHANNEL)
#define DMA_CB_CHANNEL_MASK (1u << DMA_CB_CHANNEL)
#define DMA_CHANNELS_MASK (DMA_CHANNEL_MASK | DMA_CB_CHANNEL_MASK)

static uintptr_t fragment_start[NUM_PIXELS * 4 + 1];

void __isr dma_complete_handler() {
    if (dma_hw->ints0 & DMA_CHANNEL_MASK) {
        dma_hw->ints0 = DMA_CHANNEL_MASK;
        printf("Dma transfer complete\n\r");
    }
}

void dma_init(PIO pio, uint8_t* buffer_pointer, uint dma_channel, uint sm) {
    dma_claim_mask(1u << dma_channel);

    dma_channel_config channel_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_dreq(&channel_config, pio_get_dreq(pio, sm, true));
    channel_config_set_irq_quiet(&channel_config, true);
    channel_config_set_write_increment(&channel_config, false);
    channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_8);
    dma_channel_configure(dma_channel,
                          &channel_config,
                          &pio->txf[sm],
                          buffer_pointer,
                          8, // 8 words for 8 bit planes // TODO, verify
                          false);

  irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_handler);
  dma_channel_set_irq0_enabled(DMA_CHANNEL, true);
  irq_set_enabled(DMA_IRQ_0, true);
}


int main() {
  stdio_init_all();
  watchdog_enable(PRIV_WATCHDOG_TIMEOUT, true);
  // uart_init(PRIV_UART_ID, PRIV_UART_BAUD_RATE);
  // gpio_set_function(PRIV_UART_TX_PIN, GPIO_FUNC_UART);
  // gpio_set_function(PRIV_UART_RX_PIN, GPIO_FUNC_UART);

  // int counter = 0;

  // gpio_set_function(PRIV_LEDS_GPIO_PIN, GPIO_FUNC_PWM);
  // uint slice = pwm_gpio_to_slice_num(PRIV_GPIO_PIN);
  // pwm_set_counter(slice, COUNTER);
  // pwm_set_wrap(slice, 90 - 1);
  // pwm_set_chan_level(slice, PWM_CHAN_A, HIGH_LEVEL);
  // printf("Started pwm\n");
  // pwm_set_enabled(slice, true);
  initialize_gpio(PRIV_USER_GPIO_PIN);

  PIO pio = pio0;
  int sm = 0;
  uint offset = pio_add_program(pio, &ws2812_parallel_program);

  ws2812_parallel_program_init(pio, sm, offset, WS2812_PIN_BASE, 800000);

  dma_init(pio, sm);
  int t = 0;
  while (1) {
      int dir = (rand() >> 30) & 1 ? 1 : -1;
      if (rand() & 1) dir = 0;
      puts(dir == 1 ? "(forward)" : dir ? "(backward)" : "(still)");
      for (int i = 0; i < 1000; ++i) {
          current_strip_out = strip0.data;
          current_strip_4color = false;
          pattern_random(NUM_PIXELS, t);

          sem_acquire_blocking(&reset_delay_complete_sem);
          output_strips_dma(states[current], NUM_PIXELS * 4);

          current ^= 1;
          t += dir;
      }
  }
  while (true) {
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
    watchdog_update();
    // printf("Sending by UART\n");
    sleep_ms(500);
  }
}

#define FRAC_BITS 4
#define NUM_PIXELS 64
#define WS2812_PIN_BASE 23

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

void pattern_random(uint len, uint t) {
    if (t % 8)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(rand());
}

typedef struct {
    uint8_t *data;
    uint data_len;
    uint frac_brightness; // 256 = *1.0;
} strip_t;

static uint8_t strip0_data[NUM_PIXELS * 3];

strip_t strip0 = {
        .data = strip0_data,
        .data_len = sizeof(strip0_data),
        .frac_brightness = 0x40,
};

void output_strips_dma(value_bits_t *bits, uint value_length) {
    for (uint i = 0; i < value_length; i++) {
        fragment_start[i] = (uintptr_t) bits[i].planes; // MSB first
    }
    fragment_start[value_length] = 0;
    dma_channel_hw_addr(DMA_CB_CHANNEL)->al3_read_addr_trig = (uintptr_t) fragment_start;
}
