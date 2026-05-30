/*
 * Pin-connectivity test (MSPM0 side, INPUT mode).
 *
 * Configures PB2 (SEESAW_LEFT pin 9) and PB3 (SEESAW_LEFT pin 10) as plain
 * GPIO inputs. The board's external I2C pull-ups (R1 on PB2, R4 on PB3) hold
 * them HIGH when idle, so the Fruit Jam tests a connection by driving its pin
 * LOW and we observe the corresponding PB pin drop in GPIOB.DIN over SWD.
 *
 * Read over SWD:  GPIOB.DIN31_0 @ 0x400A3380  (PB2 = bit2, PB3 = bit3)
 */
#include <stdint.h>
#include "device.h"

#define GPIOB ((GPIO_Regs *)0x400A2000U)
extern uint32_t _stack_top;

int main(void) {
    GPIOB->GPRCM.RSTCTL = RSTCTL_KEY | GPRCM_RESET;
    GPIOB->GPRCM.PWREN  = PWREN_KEY  | GPRCM_ENABLE;
    for (volatile int i = 0; i < 24; i++) __asm__ volatile("nop");
    /* PB2/PB3 as GPIO inputs (function 1), input buffer on, no internal pull
     * (external R1/R4 pull them high). */
    PINCM(PINCM_PB2_SCL) = PINCM_PC | PINCM_INENA | PINCM_PF(PF_GPIO);
    PINCM(PINCM_PB3_SDA) = PINCM_PC | PINCM_INENA | PINCM_PF(PF_GPIO);
    GPIOB->DOE31_0 = 0;                 /* all inputs */
    for (;;) __wfi();
}

__attribute__((noreturn)) void reset_handler(void) { (void)main(); for (;;) {} }
__attribute__((noreturn)) void default_handler(void) { for (;;) {} }

typedef void (*vector_t)(void);
__attribute__((section(".vectors"), used))
const vector_t vectors[] = {
    (vector_t)(&_stack_top), reset_handler, default_handler, default_handler,
    0,0,0,0, 0,0,0, default_handler, 0,0, default_handler, default_handler,
};
