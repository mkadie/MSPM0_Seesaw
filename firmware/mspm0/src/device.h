/*
 * device.h — MSPM0G3507 register map + board pin map for the seesaw firmware.
 *
 * Board: MspM0_Tiny1616 v3 (MSPM0G3507SPTRPT0048A, 48-pin LQFP).
 * Pin/PINCM/PF data resolved from SysConfig device data (MSPM0G350X.json) and
 * the v3 schematic. Notable corrections vs. mspm0_firmware_spec.md:
 *   - I2C is on **I2C1** (PB2/PB3), NOT I2C0. PB2/PB3 have no I2C0 mux.
 *       PB2 = I2C1.SCL (PINCM15, PF4),  PB3 = I2C1.SDA (PINCM16, PF4).
 *       NOTE: the schematic labels PB2's net "SDA" and PB3's net "SCL", but the
 *       silicon forces SCL=PB2/SDA=PB3 — so the host SDA/SCL wires must be
 *       crossed at the (DNP, hand-wired) SEESAW_LEFT header. Firmware is correct
 *       for the silicon regardless.
 *   - Debug UART is **UART1** (PA8=TX/PINCM19/PF2, PA9=RX/PINCM20/PF2), NOT UART0.
 */
#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include "hw_gpio.h"
#include "hw_iomux.h"
#include "hw_i2c.h"
#include "hw_uart.h"

/* --- Peripheral base pointers (typed, from the SDK structs) -------------- */
#define GPIOA   ((GPIO_Regs  *)0x400A0000U)
#define I2C1    ((I2C_Regs   *)0x400F2000U)
#define UART1   ((UART_Regs  *)0x40100000U)
#define IOMUX   ((IOMUX_Regs *)0x40428000U)

/* --- Cortex-M0+ core (CMSIS-free minimal) ------------------------------- */
#define NVIC_ISER (*(volatile uint32_t *)0xE000E100U)
#define SCB_AIRCR (*(volatile uint32_t *)0xE000ED0CU)
#define SYSRESETREQ_KEY 0x05FA0004U
static inline void NVIC_enable(uint32_t irqn) { NVIC_ISER = (1U << (irqn & 31U)); }
static inline void system_reset(void) { SCB_AIRCR = SYSRESETREQ_KEY; for (;;) {} }
static inline void __wfi(void) { __asm__ volatile ("wfi"); }
static inline void enable_irq(void) { __asm__ volatile ("cpsie i"); }

/* --- IRQ numbers (mspm0g350x.h) ----------------------------------------- */
#define GPIOA_IRQn 1   /* grouped: GPIOA/GPIOB/TRNG/COMP — we only use GPIOA */
#define I2C1_IRQn  25

/* --- GPRCM keys --------------------------------------------------------- */
#define PWREN_KEY   0x26000000U
#define RSTCTL_KEY  0xB1000000U
#define GPRCM_ENABLE 0x1U
#define GPRCM_RESET  0x3U   /* RESETASSERT | RESETSTKYCLR */

/* --- IOMUX PINCM access (1-based PINCM number, per blink convention) ----- */
#define PINCM(n)        (IOMUX->SECCFG.PINCM[(n) - 1])
#define PINCM_PC        (1U << 7)    /* peripheral connected */
#define PINCM_INENA     (1U << 18)   /* input buffer enable */
#define PINCM_PIPU      (1U << 17)   /* pull-up enable */
#define PINCM_PF(f)     ((uint32_t)(f) & 0x3FU)
#define PF_GPIO 1
#define PF_UART1 2   /* PA8/PA9 */
#define PF_I2C1 4    /* PB2/PB3 */

/* --- PINCM indices for the pins we touch -------------------------------- */
#define PINCM_PB2_SCL 15
#define PINCM_PB3_SDA 16
#define PINCM_PA8_TX  19
#define PINCM_PA9_RX  20
#define PINCM_PA10_LED 21

/* --- GPIOA bit positions (DIO numbers) ---------------------------------- */
#define LED_BIT  (1U << 10)   /* PA10, active-low (drive 0 = lit) */

/* Buttons "0".."7" -> GPIOA DIO + its PINCM (all on GPIOA, input pull-up).
 * Buttons 6/7 reuse PA9/PA8 (formerly the debug UART) — UART dropped. */
#define BTN0_DIO 27   /* PA27, PINCM60 */
#define BTN1_DIO 26   /* PA26, PINCM59 */
#define BTN2_DIO 25   /* PA25, PINCM55 */
#define BTN3_DIO 24   /* PA24, PINCM54 */
#define BTN4_DIO 22   /* PA22, PINCM47 */
#define BTN5_DIO 17   /* PA17, PINCM39 */
#define BTN6_DIO  9   /* PA9,  PINCM20 (ex-UART RX) */
#define BTN7_DIO  8   /* PA8,  PINCM19 (ex-UART TX) */
/* BTN1 (PA26 = A4) is now a normal button: FULL_POWER is controlled elsewhere
 * and A4 is an input on this board, so BTN1 gets a pull-up + falling-edge IRQ
 * like the others — see gpio_init(). (Requires the old A4 pull-down removed,
 * else it fights the pull-up.) */
#define BTN_MASK ((1U<<BTN0_DIO)|(1U<<BTN1_DIO)|(1U<<BTN2_DIO)|(1U<<BTN3_DIO)| \
                  (1U<<BTN4_DIO)|(1U<<BTN5_DIO)|(1U<<BTN6_DIO)|(1U<<BTN7_DIO))

/* Spare GPIO "11".."16" -> GPIOA DIO + PINCM (host-controllable). */
#define SP11_DIO 18   /* PA18, PINCM40 */
#define SP12_DIO 12   /* PA12, PINCM34 */
#define SP13_DIO 13   /* PA13, PINCM35 */
#define SP14_DIO 14   /* PA14, PINCM36 */
#define SP15_DIO 15   /* PA15, PINCM37 */
#define SP16_DIO 16   /* PA16, PINCM38 */

#endif /* DEVICE_H */
