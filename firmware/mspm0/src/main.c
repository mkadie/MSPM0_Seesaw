/*
 * MSPM0G3507 seesaw firmware — MspM0_Tiny1616 v3 board.
 *
 * Adafruit-style Seesaw I2C peripheral. See mspm0_firmware_spec.md.
 * Modules: STATUS (0x00), GPIO (0x01), EVENT (0x80).
 * I2C target @0x49 on I2C1 (PB2=SCL, PB3=SDA). Debug UART1 @115200 (PA8 TX).
 *
 * Bare-metal: register access via the SDK hw_*.h structs (see device.h).
 */
#include <stdint.h>
#include "device.h"

/* ====================================================================== */
/* Identity / protocol constants                                          */
/* ====================================================================== */
#define I2C_ADDR        0x49u

#define MOD_STATUS      0x00u
#define  FN_HW_ID       0x01u
#define  FN_VERSION     0x02u
#define  FN_OPTIONS     0x03u
#define  FN_SELFTEST    0x04u
#define  FN_SWRST       0x7Fu
#define MOD_GPIO        0x01u
#define  FN_DIRSET      0x02u
#define  FN_DIRCLR      0x03u
#define  FN_BULK        0x04u
#define  FN_BULK_SET    0x05u
#define  FN_BULK_CLR    0x06u
#define  FN_INTFLAG     0x0Au
#define  FN_PULLENSET   0x0Bu
#define  FN_PULLENCLR   0x0Cu
#define MOD_EVENT       0x80u
#define  FN_EVENT_LEN   0x01u
#define  FN_EVENT_POPALL 0x02u

#define HW_ID_VALUE     0x84u
#define VERSION_VALUE   0x00020100u
#define OPTIONS_VALUE   0x00000007u
#define SELFTEST_VALUE  0x55u
#define SELFTEST_PATTERN 0x0000003Fu

/* ====================================================================== */
/* Logical host-GPIO bit map: bit -> GPIOA DIO (0xFF = unmapped)          */
/* Buttons "0".."5" = bits 0..5 (read-only inputs).                       */
/* Spare "11".."16" = bits 11..16 (host-writable).                        */
/* ====================================================================== */
#define NLOGBITS 17
static const uint8_t logbit_dio[NLOGBITS] = {
    BTN0_DIO, BTN1_DIO, BTN2_DIO, BTN3_DIO, BTN4_DIO, BTN5_DIO, /* 0..5 */
    BTN6_DIO, BTN7_DIO,                                         /* 6,7 (PA9/PA8) */
    0xFF, 0xFF, 0xFF,                                           /* 8..10 */
    SP11_DIO, SP12_DIO, SP13_DIO, SP14_DIO, SP15_DIO, SP16_DIO, /* 11..16 */
};
/* PINCM per spare logical bit (for pull-enable); buttons not host-writable. */
static const uint8_t sp_pincm[NLOGBITS] = {
    0,0,0,0,0,0, 0,0,0,0,0, 40,34,35,36,37,38,
};
/* Host-writable logical bits: spare 11..16 AND buttons 0..7. Buttons default to
 * firmware-managed inputs, but the host may reconfigure them as outputs (needed
 * for the FJ<->seesaw loopback test and general Seesaw-GPIO flexibility). */
#define HOST_WRITABLE_MASK 0x0001F8FFu  /* logical bits 0..7 and 11..16 */

/* button DIO -> human index 0..7 for the event FIFO */
static uint8_t btn_dio_to_index(uint32_t dio) {
    switch (dio) {
    case BTN0_DIO: return 0; case BTN1_DIO: return 1; case BTN2_DIO: return 2;
    case BTN3_DIO: return 3; case BTN4_DIO: return 4; case BTN5_DIO: return 5;
    case BTN6_DIO: return 6; case BTN7_DIO: return 7;
    default: return 0xFF;
    }
}

/* ====================================================================== */
/* Shared state (touched by ISRs — keep volatile)                         */
/* ====================================================================== */
static volatile uint32_t gpio_intflag = 0;     /* latched button bitfield */

/* 16-deep event ring of pin numbers / markers */
#define EVT_CAP 16
static volatile uint8_t  evt_buf[EVT_CAP];
static volatile uint8_t  evt_head = 0, evt_tail = 0, evt_count = 0;
static volatile uint16_t evt_dropped = 0;

static void evt_push(uint8_t v) {
    if (evt_count >= EVT_CAP) { evt_dropped++; return; }
    evt_buf[evt_tail] = v;
    evt_tail = (uint8_t)((evt_tail + 1) % EVT_CAP);
    evt_count++;
}
static uint8_t evt_pop(void) {
    uint8_t v = evt_buf[evt_head];
    evt_head = (uint8_t)((evt_head + 1) % EVT_CAP);
    evt_count--;
    return v;
}

/* ====================================================================== */
/* LED (PA10, active-low: drive 0 = lit, 1 = off)                         */
/* ====================================================================== */
static inline void led_on(void)  { GPIOA->DOUTCLR31_0 = LED_BIT; }
static inline void led_off(void) { GPIOA->DOUTSET31_0 = LED_BIT; }

/* Debug UART dropped — PA8/PA9 are now button inputs (buttons 7/6). */

/* ====================================================================== */
/* GPIO init: LED output, buttons input+pullup+falling IRQ, spare input   */
/* ====================================================================== */
static void gpio_init(void) {
    GPIOA->GPRCM.RSTCTL = RSTCTL_KEY | GPRCM_RESET;
    GPIOA->GPRCM.PWREN  = PWREN_KEY  | GPRCM_ENABLE;
    for (volatile int i = 0; i < 24; i++) { __asm__ volatile("nop"); }

    /* LED: PA10 GPIO output, start extinguished (high) */
    PINCM(PINCM_PA10_LED) = PINCM_PC | PINCM_PF(PF_GPIO);
    GPIOA->DOUTSET31_0 = LED_BIT;
    GPIOA->DOE31_0    |= LED_BIT;

    /* Buttons 0..7: input + internal pull-up + GPIO function.
     * (buttons 6/7 = PA9/PA8 = PINCM20/19, ex-UART). */
    static const uint8_t btn_pincm[8] = { 60, 59, 55, 54, 47, 39, 20, 19 };
    for (int i = 0; i < 8; i++) {
        PINCM(btn_pincm[i]) = PINCM_PC | PINCM_INENA | PINCM_PIPU | PINCM_PF(PF_GPIO);
    }
    /* PA26 (PINCM59 = A4) is now a normal button — the btn_pincm loop above
     * already gave it input + internal pull-up. FULL_POWER is controlled
     * elsewhere; if the old external A4 pull-down is still fitted it fights this
     * pull-up, so it must be removed for BTN1 to read cleanly. */
    /* Spare "11".."16": input, no pull (host may reconfigure) */
    for (int b = 11; b <= 16; b++) {
        PINCM(sp_pincm[b]) = PINCM_PC | PINCM_INENA | PINCM_PF(PF_GPIO);
    }

    /* Falling-edge detect (2 bits/DIO, FALL = 0b10).
     * DIOs 17..27 live in POLARITY31_16 (field at (dio-16)*2);
     * DIOs 8/9 (PA8/PA9) live in POLARITY15_0 (field at dio*2). */
    uint32_t pol_hi = 0;
    static const uint8_t btn_dio_hi[6] =   /* PA17..PA27 buttons (incl. BTN1/A4) */
        { BTN0_DIO, BTN1_DIO, BTN2_DIO, BTN3_DIO, BTN4_DIO, BTN5_DIO };
    for (int i = 0; i < 6; i++) {
        pol_hi |= (2u << ((btn_dio_hi[i] - 16) * 2));
    }
    GPIOA->POLARITY31_16 = pol_hi;
    GPIOA->POLARITY15_0  = (2u << (BTN6_DIO * 2)) | (2u << (BTN7_DIO * 2));

    GPIOA->CPU_INT.ICLR  = BTN_MASK;   /* clear any stale */
    GPIOA->CPU_INT.IMASK = BTN_MASK;   /* enable button interrupts */
}

/* ====================================================================== */
/* Read current host-GPIO input states into a 32-bit Seesaw bulk value    */
/* ====================================================================== */
static uint32_t gpio_bulk_read(void) {
    uint32_t din = GPIOA->DIN31_0;
    uint32_t v = 0;
    for (int b = 0; b < NLOGBITS; b++) {
        uint8_t dio = logbit_dio[b];
        if (dio != 0xFF && (din & (1u << dio))) v |= (1u << b);
    }
    return v;
}
/* Apply a logical-bit mask to GPIOA (set/clear output, or direction). */
static void gpio_apply(uint32_t logmask, int action) {
    logmask &= HOST_WRITABLE_MASK;     /* protect buttons + reserved pins */
    for (int b = 0; b < NLOGBITS; b++) {
        if (!(logmask & (1u << b))) continue;
        uint32_t dio = logbit_dio[b];
        if (dio == 0xFF) continue;
        uint32_t m = (1u << dio);
        int is_btn = (b <= 7);                          /* bits 0..7 have edge IRQs */
        switch (action) {
        case 0: GPIOA->DOUTSET31_0 = m; break;          /* BULK_SET */
        case 1: GPIOA->DOUTCLR31_0 = m; break;          /* BULK_CLR */
        case 2: /* DIRSET (output): suppress this pin's button IRQ first */
            if (is_btn) GPIOA->CPU_INT.IMASK &= ~m;
            GPIOA->DOE31_0 |= m;
            break;
        case 3: /* DIRCLR (input): restore input, re-arm button IRQ */
            GPIOA->DOE31_0 &= ~m;
            if (is_btn) { GPIOA->CPU_INT.ICLR = m; GPIOA->CPU_INT.IMASK |= m; }
            break;
        case 4: PINCM(sp_pincm[b]) |= PINCM_PIPU; break;/* PULLENSET */
        case 5: PINCM(sp_pincm[b]) &= ~PINCM_PIPU; break;/* PULLENCLR */
        }
    }
}

/* ====================================================================== */
/* I2C target transaction state + Seesaw command handling                 */
/* ====================================================================== */
#define RXBUF_CAP 24
static volatile uint8_t  rx_buf[RXBUF_CAP];
static volatile uint8_t  rx_idx = 0;
static volatile uint8_t  tx_buf[20];
static volatile uint8_t  tx_len = 0;
static volatile uint8_t  tx_idx = 0;

static void put_be32(volatile uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v;
}

/* Prepare the read response for <module,func> (runs once MODULE+FUNC are in). */
static void prepare_response(uint8_t mod, uint8_t fn) {
    tx_len = 0; tx_idx = 0;
    if (mod == MOD_STATUS) {
        switch (fn) {
        case FN_HW_ID:    tx_buf[0] = HW_ID_VALUE; tx_len = 1; break;
        case FN_VERSION:  put_be32(tx_buf, VERSION_VALUE); tx_len = 4; break;
        case FN_OPTIONS:  put_be32(tx_buf, OPTIONS_VALUE); tx_len = 4; break;
        case FN_SELFTEST:
            gpio_intflag = SELFTEST_PATTERN;
            evt_push(0xFE); evt_push(0xED);
            led_on();
            tx_buf[0] = SELFTEST_VALUE; tx_len = 1; break;
        default: break;
        }
    } else if (mod == MOD_GPIO) {
        switch (fn) {
        case FN_BULK:     put_be32(tx_buf, gpio_bulk_read()); tx_len = 4; break;
        case FN_INTFLAG:
            put_be32(tx_buf, gpio_intflag); tx_len = 4;
            gpio_intflag = 0; led_off();       /* read clears + LED off */
            break;
        default: break;
        }
    } else if (mod == MOD_EVENT) {
        switch (fn) {
        case FN_EVENT_LEN: tx_buf[0] = evt_count; tx_len = 1; break;
        case FN_EVENT_POPALL: {
            uint8_t n = 0;
            while (evt_count && n < sizeof(tx_buf)) tx_buf[n++] = evt_pop();
            tx_len = n; led_off();             /* drained -> LED off */
            break;
        }
        default: break;
        }
    }
}

/* Apply a write command on STOP (mod,func + optional 4-byte data). */
static void apply_write(uint8_t mod, uint8_t fn, volatile uint8_t *d, uint8_t n) {
    uint32_t arg = 0;
    if (n >= 4) arg = ((uint32_t)d[0]<<24)|((uint32_t)d[1]<<16)|((uint32_t)d[2]<<8)|d[3];
    if (mod == MOD_STATUS && fn == FN_SWRST) {
        if (n >= 1 && d[0] == 0xFF) system_reset();
    } else if (mod == MOD_GPIO) {
        switch (fn) {
        case FN_BULK_SET:   gpio_apply(arg, 0); break;
        case FN_BULK_CLR:   gpio_apply(arg, 1); break;
        case FN_DIRSET:     gpio_apply(arg, 2); break;
        case FN_DIRCLR:     gpio_apply(arg, 3); break;
        case FN_PULLENSET:  gpio_apply(arg, 4); break;
        case FN_PULLENCLR:  gpio_apply(arg, 5); break;
        default: break;
        }
    }
}

static void i2c_init(void) {
    I2C1->GPRCM.RSTCTL = RSTCTL_KEY | GPRCM_RESET;
    I2C1->GPRCM.PWREN  = PWREN_KEY  | GPRCM_ENABLE;
    for (volatile int i = 0; i < 24; i++) { __asm__ volatile("nop"); }

    /* Select the I2C functional clock (BUSCLK, no divide) — without a clock the
     * target FSM can't detect/ACK its address. */
    I2C1->CLKSEL = I2C_CLKSEL_BUSCLK_SEL_ENABLE;
    I2C1->CLKDIV = I2C_CLKDIV_RATIO_DIV_BY_1;

    /* PB2 = I2C1.SCL, PB3 = I2C1.SDA (PF4). Input buffer enabled; the I2C
     * peripheral drives open-drain. External pull-ups (R1/R4) on board. */
    PINCM(PINCM_PB2_SCL) = PINCM_PC | PINCM_INENA | PINCM_PF(PF_I2C1);
    PINCM(PINCM_PB3_SDA) = PINCM_PC | PINCM_INENA | PINCM_PF(PF_I2C1);

    /* Own address 0x49 AND enable address matching (OAREN) — SOAR value alone
     * does nothing without OAREN. */
    I2C1->SLAVE.SOAR = I2C_SOAR_OAREN_ENABLE | I2C_ADDR;
    /* Target active, clock-stretch while preparing data, TX-empty IRQ on TREQ. */
    I2C1->SLAVE.SCTR = I2C_SCTR_ACTIVE_ENABLE | I2C_SCTR_SCLKSTRETCH_ENABLE
                     | I2C_SCTR_TXEMPTY_ON_TREQ_ENABLE;

    /* Enable target interrupts: START, STOP, RX byte, TX-needs-byte. */
    I2C1->CPU_INT.IMASK = I2C_CPU_INT_IMASK_SSTART_SET
                        | I2C_CPU_INT_IMASK_SSTOP_SET
                        | I2C_CPU_INT_IMASK_SRXDONE_SET
                        | I2C_CPU_INT_IMASK_STXEMPTY_SET;
}

/* ====================================================================== */
/* ISRs                                                                   */
/* ====================================================================== */
void I2C1_IRQHandler(void) {
    uint32_t mis = I2C1->CPU_INT.MIS;

    if (mis & I2C_CPU_INT_IMASK_SSTART_SET) {
        /* (repeated) START — do not reset here; STOP frames the transaction. */
    }
    if (mis & I2C_CPU_INT_IMASK_SRXDONE_SET) {
        uint8_t byte = (uint8_t)I2C1->SLAVE.SRXDATA;
        if (rx_idx < RXBUF_CAP) rx_buf[rx_idx++] = byte;
        if (rx_idx == 2) prepare_response(rx_buf[0], rx_buf[1]);
    }
    if (mis & I2C_CPU_INT_IMASK_STXEMPTY_SET) {
        /* Controller is reading: always feed a byte (0x00 past the response). */
        uint8_t b = (tx_idx < tx_len) ? tx_buf[tx_idx] : 0x00u;
        if (tx_idx < tx_len) tx_idx++;
        I2C1->SLAVE.STXDATA = b;
    }
    if (mis & I2C_CPU_INT_IMASK_SSTOP_SET) {
        /* A command with data bytes (rx_idx > 2) is a write -> apply it.
         * A 2-byte command (rx_idx == 2) is a read setup: KEEP the prepared
         * tx_buf/tx_len so a following separate read transaction (host pattern
         * writeto()+readfrom_into(), which inserts a STOP) still gets it.
         * Only rx_idx is reset here; prepare_response() resets tx_idx/tx_len
         * on the next command. */
        if (rx_idx > 2) apply_write(rx_buf[0], rx_buf[1], &rx_buf[2],
                                    (uint8_t)(rx_idx - 2));
        rx_idx = 0;
    }
    I2C1->CPU_INT.ICLR = mis;
}

void GPIOA_IRQHandler(void) {
    uint32_t ris = GPIOA->CPU_INT.RIS & BTN_MASK;
    GPIOA->CPU_INT.ICLR = ris;
    if (!ris) return;
    for (int dio = 0; dio < 32; dio++) {
        if (!(ris & (1u << dio))) continue;
        uint8_t idx = btn_dio_to_index(dio);   /* 0..5 = logical bit = event id */
        if (idx == 0xFF) continue;
        gpio_intflag |= (1u << idx);
        evt_push(idx);
    }
    led_on();
}

/* ====================================================================== */
/* main                                                                   */
/* ====================================================================== */
int main(void) {
    gpio_init();
    i2c_init();

    NVIC_enable(GPIOA_IRQn);
    NVIC_enable(I2C1_IRQn);
    enable_irq();

    for (;;) { __wfi(); }
}

/* ====================================================================== */
/* Startup: reset handler + vector table                                  */
/* ====================================================================== */
extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

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
    (vector_t)(&_stack_top),  /*  0 SP                   */
    reset_handler,            /*  1 Reset                */
    default_handler,          /*  2 NMI                  */
    default_handler,          /*  3 HardFault            */
    0,0,0,0, 0,0,0,           /*  4..10 reserved         */
    default_handler,          /* 11 SVCall               */
    0,0,                      /* 12..13 reserved         */
    default_handler,          /* 14 PendSV               */
    default_handler,          /* 15 SysTick              */
    /* --- external IRQs 0.. --- */
    default_handler,          /* 16 IRQ0  SYSCTL/WWDT... */
    GPIOA_IRQHandler,         /* 17 IRQ1  GPIOA group    */
    default_handler,          /* 18 IRQ2  TIMG8          */
    default_handler,          /* 19 IRQ3  UART3          */
    default_handler,          /* 20 IRQ4  ADC0           */
    default_handler,          /* 21 IRQ5  ADC1           */
    default_handler,          /* 22 IRQ6  CANFD0         */
    default_handler,          /* 23 IRQ7  DAC0           */
    default_handler,          /* 24 IRQ8                 */
    default_handler,          /* 25 IRQ9  SPI0           */
    default_handler,          /* 26 IRQ10 SPI1           */
    default_handler,          /* 27 IRQ11                */
    default_handler,          /* 28 IRQ12                */
    default_handler,          /* 29 IRQ13 UART1          */
    default_handler,          /* 30 IRQ14 UART2          */
    default_handler,          /* 31 IRQ15 UART0          */
    default_handler,          /* 32 IRQ16 TIMG0          */
    default_handler,          /* 33 IRQ17 TIMG6          */
    default_handler,          /* 34 IRQ18 TIMA0          */
    default_handler,          /* 35 IRQ19 TIMA1          */
    default_handler,          /* 36 IRQ20 TIMG7          */
    default_handler,          /* 37 IRQ21 TIMG12         */
    default_handler,          /* 38 IRQ22                */
    default_handler,          /* 39 IRQ23                */
    default_handler,          /* 40 IRQ24 I2C0           */
    I2C1_IRQHandler,          /* 41 IRQ25 I2C1           */
};
