/*
 * I2C-activity LED diagnostic for the MspM0_Tiny1616 v3 board.
 *
 * LED (PA10, active-low) starts OFF. The I2C1 target is set up at 0x49 exactly
 * like the seesaw firmware, but the only job here is: on ANY I2C interrupt
 * (START/STOP/byte), latch the LED ON. So:
 *   - LED stays dark during a host I2C scan  -> SDA/SCL not reaching the chip.
 *   - LED lights but 0x49 absent in the scan -> bus reaches chip, addressing
 *     wrong (most likely SDA/SCL swapped).
 *   - LED lights AND 0x49 shows in the scan  -> wiring good.
 *
 * Build (standalone, not via the default Makefile target):
 *   arm-none-eabi-gcc <CFLAGS> -c src/i2c_diag.c -o build/i2c_diag.o
 *   arm-none-eabi-gcc <LDFLAGS> build/i2c_diag.o -o build/i2c_diag.elf
 */
#include <stdint.h>
#include "device.h"

#define I2C_ADDR 0x49u

static inline void led_on(void)  { GPIOA->DOUTCLR31_0 = LED_BIT; }
static inline void led_off(void) { GPIOA->DOUTSET31_0 = LED_BIT; }

static void gpio_init(void) {
    GPIOA->GPRCM.RSTCTL = RSTCTL_KEY | GPRCM_RESET;
    GPIOA->GPRCM.PWREN  = PWREN_KEY  | GPRCM_ENABLE;
    for (volatile int i = 0; i < 24; i++) __asm__ volatile("nop");
    PINCM(PINCM_PA10_LED) = PINCM_PC | PINCM_PF(PF_GPIO);
    led_off();
    GPIOA->DOE31_0 |= LED_BIT;
}

static void i2c_init(void) {
    I2C1->GPRCM.RSTCTL = RSTCTL_KEY | GPRCM_RESET;
    I2C1->GPRCM.PWREN  = PWREN_KEY  | GPRCM_ENABLE;
    for (volatile int i = 0; i < 24; i++) __asm__ volatile("nop");
    PINCM(PINCM_PB2_SCL) = PINCM_PC | PINCM_INENA | PINCM_PF(PF_I2C1);
    PINCM(PINCM_PB3_SDA) = PINCM_PC | PINCM_INENA | PINCM_PF(PF_I2C1);
    I2C1->SLAVE.SOAR = I2C_ADDR;
    I2C1->SLAVE.SCTR = I2C_SCTR_ACTIVE_ENABLE;
    /* fire on START, STOP, RX byte — any of them means bus activity reached us */
    I2C1->CPU_INT.IMASK = I2C_CPU_INT_IMASK_SSTART_SET
                        | I2C_CPU_INT_IMASK_SSTOP_SET
                        | I2C_CPU_INT_IMASK_SRXDONE_SET;
}

void I2C1_IRQHandler(void) {
    uint32_t mis = I2C1->CPU_INT.MIS;
    led_on();                        /* latch: we heard the bus */
    (void)I2C1->SLAVE.SRXDATA;       /* drain any byte so RX doesn't stall */
    I2C1->CPU_INT.ICLR = mis;
}

int main(void) {
    gpio_init();
    i2c_init();
    NVIC_enable(I2C1_IRQn);
    enable_irq();
    for (;;) __wfi();
}

extern uint32_t _stack_top;
__attribute__((noreturn)) void reset_handler(void) { (void)main(); for (;;) {} }
__attribute__((noreturn)) void default_handler(void) { for (;;) {} }

typedef void (*vector_t)(void);
__attribute__((section(".vectors"), used))
const vector_t vectors[] = {
    (vector_t)(&_stack_top), reset_handler, default_handler, default_handler,
    0,0,0,0, 0,0,0, default_handler, 0,0, default_handler, default_handler,
    default_handler,          /* 16 IRQ0  */
    default_handler,          /* 17 IRQ1  GPIOA */
    default_handler,default_handler,default_handler,default_handler,default_handler,
    default_handler,default_handler,default_handler,default_handler,default_handler,
    default_handler,default_handler,default_handler,default_handler,default_handler,
    default_handler,default_handler,default_handler,default_handler,default_handler,
    default_handler,default_handler,default_handler,default_handler, /* ..IRQ24 */
    I2C1_IRQHandler,          /* 41 IRQ25 I2C1 */
};
