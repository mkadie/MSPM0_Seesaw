/*
 * MSPM0G3507 — LCD backlight toggle test (standalone, NOT the seesaw firmware).
 *
 * Drives FULL_POWER on PA16 (logical bit 16): active-low to a current-limited
 * load switch — drive LOW = LCD rail ON, HIGH = OFF. Pattern (repeats forever):
 *   ON 1s, OFF 1s, ON 1s, OFF 1s   (i.e. on/off twice), then a 3s gap.
 *
 * Build:  see one-off gcc/objcopy commands. Flash like firmware.elf.
 * Restore the real firmware afterward: flash build/firmware.elf.
 */
#include <stdint.h>
#include "device.h"

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

/* SysTick — 1 ms ticks assuming 32 MHz MCLK (MSPM0G reset default = SYSOSC 32M).
 * If the on/off holds aren't ~1 s, MCLK isn't 32 MHz; adjust TICKS_PER_MS. */
#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)
#define TICKS_PER_MS 32000u

static void delay_ms(uint32_t ms) {
    SYST_RVR = TICKS_PER_MS - 1u;
    SYST_CVR = 0u;
    SYST_CSR = 5u;                              /* ENABLE | CLKSOURCE=processor */
    while (ms--) {
        while (!(SYST_CSR & (1u << 16))) { }    /* wait COUNTFLAG (1 ms) */
    }
    SYST_CSR = 0u;
}

static inline void bl_on(void)  { GPIOA->DOUTCLR31_0 = FULLPWR_BIT; }  /* low = on  */
static inline void bl_off(void) { GPIOA->DOUTSET31_0 = FULLPWR_BIT; }  /* high = off */

int main(void) {
    /* Power up GPIOA */
    GPIOA->GPRCM.RSTCTL = RSTCTL_KEY | GPRCM_RESET;
    GPIOA->GPRCM.PWREN  = PWREN_KEY  | GPRCM_ENABLE;
    for (volatile int i = 0; i < 24; i++) { __asm__ volatile("nop"); }

    /* PA16 (PINCM38) = FULL_POWER enable, GPIO output, start OFF (high). */
    bl_off();
    PINCM(FULLPWR_PINCM) = PINCM_PC | PINCM_INENA | PINCM_PF(PF_GPIO);
    GPIOA->DOE31_0 |= FULLPWR_BIT;

    for (;;) {
        for (int k = 0; k < 2; k++) {          /* on/off twice */
            bl_on();  delay_ms(1000);
            bl_off(); delay_ms(1000);
        }
        delay_ms(3000);                        /* gap, then repeat */
    }
}

__attribute__((noreturn)) void reset_handler(void) {
    uint32_t *src = &_data_load, *dst = &_data_start;
    while (dst < &_data_end) *dst++ = *src++;
    for (dst = &_bss_start; dst < &_bss_end; ) *dst++ = 0;
    (void)main();
    for (;;) { }
}
__attribute__((noreturn)) void default_handler(void) { for (;;) { } }

typedef void (*vector_t)(void);
__attribute__((section(".vectors"), used))
const vector_t vectors[] = {
    (vector_t)(&_stack_top),  /* 0 SP    */
    reset_handler,            /* 1 Reset */
    default_handler,          /* 2 NMI   */
    default_handler,          /* 3 HardFault */
};
