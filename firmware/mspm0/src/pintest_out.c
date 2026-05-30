/*
 * Pin-connectivity test (MSPM0 side, OUTPUT mode).
 *
 * Drives PB2 and PB3 as GPIO push-pull outputs in OPPOSITE phase, ~300 ms each
 * half: (PB2=0,PB3=1) then (PB2=1,PB3=0), repeating. The Fruit Jam reads D9/D10
 * as inputs and should see them toggle in opposite phase -> confirms the MSPM0
 * can drive each line and the Fruit Jam can sense it (the I2C ACK direction).
 *
 * Expected mapping (from Test A): D9<->PB3, D10<->PB2.
 */
#include <stdint.h>
#include "device.h"

#define GPIOB ((GPIO_Regs *)0x400A2000U)
#define PB2_BIT (1u << 2)
#define PB3_BIT (1u << 3)
extern uint32_t _stack_top;

static void delay(volatile uint32_t n) { while (n--) __asm__ volatile("nop"); }

int main(void) {
    GPIOB->GPRCM.RSTCTL = RSTCTL_KEY | GPRCM_RESET;
    GPIOB->GPRCM.PWREN  = PWREN_KEY  | GPRCM_ENABLE;
    for (volatile int i = 0; i < 24; i++) __asm__ volatile("nop");
    PINCM(PINCM_PB2_SCL) = PINCM_PC | PINCM_PF(PF_GPIO);   /* GPIO output */
    PINCM(PINCM_PB3_SDA) = PINCM_PC | PINCM_PF(PF_GPIO);
    GPIOB->DOE31_0 |= (PB2_BIT | PB3_BIT);                 /* enable outputs */
    for (;;) {
        GPIOB->DOUTCLR31_0 = PB2_BIT;                      /* PB2=0 */
        GPIOB->DOUTSET31_0 = PB3_BIT;                      /* PB3=1 */
        delay(3200000);                                    /* ~300 ms @32MHz */
        GPIOB->DOUTSET31_0 = PB2_BIT;                      /* PB2=1 */
        GPIOB->DOUTCLR31_0 = PB3_BIT;                      /* PB3=0 */
        delay(3200000);
    }
}

__attribute__((noreturn)) void reset_handler(void) { (void)main(); for (;;) {} }
__attribute__((noreturn)) void default_handler(void) { for (;;) {} }

typedef void (*vector_t)(void);
__attribute__((section(".vectors"), used))
const vector_t vectors[] = {
    (vector_t)(&_stack_top), reset_handler, default_handler, default_handler,
    0,0,0,0, 0,0,0, default_handler, 0,0, default_handler, default_handler,
};
