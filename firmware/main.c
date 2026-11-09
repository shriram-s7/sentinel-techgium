/* Minimal STM32G030 sensor node: reads an NTC thermistor on the ADC, the die temperature,
 * INA219 supply current over I2C, and prints one line per second over USART2.
 * Build with -DUSE_SENTINEL to add self-diagnosis (watchdog, baseline in flash, health byte). */
#include <stdint.h>
#ifdef USE_SENTINEL
#include "sentinel.h"
#endif
#define REG(a) (*(volatile uint32_t *)(a))
#define RCC_IOPENR  REG(0x40021034)
#define RCC_APBENR1 REG(0x4002103C)
#define RCC_APBENR2 REG(0x40021040)
#define GPIOA_MODER REG(0x50000000)
#define GPIOA_AFRL  REG(0x50000020)
#define GPIOB_MODER REG(0x50000400)
#define GPIOB_OTYPER REG(0x50000404)
#define GPIOB_AFRL  REG(0x50000420)
#define USART2_CR1  REG(0x40004400)
#define USART2_BRR  REG(0x4000440C)
#define USART2_ISR  REG(0x4000441C)
#define USART2_TDR  REG(0x40004428)
#define I2C1_CR1    REG(0x40005400)
#define I2C1_CR2    REG(0x40005404)
#define I2C1_TIMINGR REG(0x40005410)
#define I2C1_ISR    REG(0x40005418)
#define I2C1_ICR    REG(0x4000541C)
#define I2C1_RXDR   REG(0x40005424)
#define I2C1_TXDR   REG(0x40005428)
#define ADC_ISR     REG(0x40012400)
#define ADC_CR      REG(0x40012408)
#define ADC_CHSELR  REG(0x40012428)
#define ADC_DR      REG(0x40012440)
#define ADC_CCR     REG(0x40012708)
#define SYST_CSR    REG(0xE000E010)
#define SYST_RVR    REG(0xE000E014)
#define IWDG_KR     REG(0x40003000)
#define IWDG_PR     REG(0x40003004)
#define IWDG_RLR    REG(0x40003008)
#define RCC_CSR     REG(0x40021060)
#define FLASH_KEYR  REG(0x40022008)
#define FLASH_SR    REG(0x40022010)
#define FLASH_CR    REG(0x40022014)
#define BASE_PAGE   15u
#define BASE_ADDR   (0x08000000u + BASE_PAGE * 2048u)
#define TS_CAL1     (*(volatile uint16_t *)0x1FFF75A8)
#define VREF_CAL    (*(volatile uint16_t *)0x1FFF75AA)

static volatile uint32_t ticks;
void SysTick_Handler(void) { ticks++; }

static void uart_putc(char c) { while (!(USART2_ISR & (1u << 7))); USART2_TDR = (uint8_t)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_hex8(uint8_t v) { const char *h = "0123456789abcdef"; uart_putc(h[v >> 4]); uart_putc(h[v & 15]); }
static void uart_int(int32_t v) {
    char b[12]; int i = 0; if (v < 0) { uart_putc('-'); v = -v; }
    do { b[i++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (i) uart_putc(b[--i]);
}

static int i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n) {
    uint32_t t;
    I2C1_CR2 = (addr << 1) | (1u << 16) | (1u << 13);              /* write 1 byte, START */
    for (t = 0; !(I2C1_ISR & (1u << 1)); ) if ((I2C1_ISR & (1u << 4)) || ++t > 20000) goto fail;
    I2C1_TXDR = reg;
    for (t = 0; !(I2C1_ISR & (1u << 6)); ) if ((I2C1_ISR & (1u << 4)) || ++t > 20000) goto fail;
    I2C1_CR2 = (addr << 1) | (1u << 10) | ((uint32_t)n << 16) | (1u << 25) | (1u << 13);
    for (uint8_t i = 0; i < n; i++) {
        for (t = 0; !(I2C1_ISR & (1u << 2)); ) if ((I2C1_ISR & (1u << 4)) || ++t > 20000) goto fail;
        buf[i] = (uint8_t)I2C1_RXDR;
    }
    return 1;
fail:
    I2C1_ICR = (1u << 4) | (1u << 5); return 0;
}

static uint16_t adc_read(uint8_t ch) {
    ADC_CHSELR = 1u << ch; ADC_CR |= 1u << 2; while (!(ADC_ISR & (1u << 2))); return (uint16_t)ADC_DR;
}

#ifdef USE_SENTINEL
#include "sentinel.h"
static void flash_save(const void *src, unsigned n) {          /* one page erase, written once per install */
    const uint32_t *w = (const uint32_t *)src; volatile uint32_t *dst = (volatile uint32_t *)BASE_ADDR;
    FLASH_KEYR = 0x45670123; FLASH_KEYR = 0xCDEF89AB;
    while (FLASH_SR & (1u << 16));
    FLASH_CR = (1u << 1) | (BASE_PAGE << 3); FLASH_CR |= 1u << 16;   /* page erase */
    while (FLASH_SR & (1u << 16));
    FLASH_CR = 1u;                                                   /* program, 64 bits at a time */
    for (unsigned i = 0; i < (n + 7) / 8; i++) {
        dst[2 * i] = w[2 * i]; dst[2 * i + 1] = w[2 * i + 1];
        while (FLASH_SR & (1u << 16));
    }
    FLASH_CR = 1u << 31;                                             /* lock */
}
#endif

static void hw_init(void) {
    RCC_IOPENR |= 3; RCC_APBENR1 |= (1u << 17) | (1u << 21); RCC_APBENR2 |= 1u << 20;
    GPIOA_MODER = (GPIOA_MODER & ~(0xFu << 4)) | (0xAu << 4); GPIOA_AFRL |= 0x1100u << 4;
    GPIOB_MODER = (GPIOB_MODER & ~(0xFu << 12)) | (0xAu << 12); GPIOB_OTYPER |= 3u << 6; GPIOB_AFRL |= 0x66u << 24;
    USART2_BRR = 16000000u / 115200u; USART2_CR1 = 1u | (1u << 3);
    I2C1_TIMINGR = 0x00303D5B; I2C1_CR1 = 1;
    GPIOA_MODER |= 3u;                                               /* PA0 analog for the NTC */
    ADC_CCR |= (1u << 23) | (1u << 22); ADC_CR |= 1u << 28; for (volatile int i = 0; i < 400; i++); ADC_CR |= 1;
    SYST_RVR = 16000 - 1; SYST_CSR = 7;
}

/* 10k NTC (B3950) with a 10k pull-up at 3.3 V: ADC counts every 10 degC from -10 to 90 C */
static const uint16_t ntc_lut[11] = { 3495, 3156, 2738, 2278, 1825, 1419, 1081, 815, 613, 462, 350 };
static int16_t ntc_cC(uint16_t raw) {
    if (raw >= ntc_lut[0]) return -1000;
    for (uint8_t i = 1; i < 11; i++)
        if (raw > ntc_lut[i]) return (int16_t)(-1000 + (i - 1) * 1000 + (int32_t)(ntc_lut[i - 1] - raw) * 1000 / (ntc_lut[i - 1] - ntc_lut[i]));
    return 9000;
}

static int16_t die_temp_cC(uint16_t raw, uint16_t vdd_mV) {
    int32_t adj = (int32_t)raw * vdd_mV / 3000;          /* TS_CAL1 taken at 3.0 V, 30 C */
    return (int16_t)(3000 + (adj - TS_CAL1) * 100 * 100 / 253);
}

int main(void) {
    hw_init();
#ifdef USE_SENTINEL
    sn_init();
    extern uint32_t __stack_canary; __stack_canary = 0xC0DEFACEuL;
    uint32_t csr = RCC_CSR; RCC_CSR |= 1u << 23;                     /* read, then clear reset flags */
    sn_note_reset(((csr >> 29) & 1u ? RST_WATCHDOG : 0) | ((csr >> 27) & 1u ? RST_POWER : 0));
    uint8_t base_saved = sn_baseline_set((const sn_baseline_t *)BASE_ADDR);   /* reuse stored normal */
    IWDG_KR = 0xCCCC; IWDG_KR = 0x5555; IWDG_PR = 4; IWDG_RLR = 1000;  /* ~2 s independent watchdog */

#endif
    uint32_t next = 0;
    for (;;) {
        while ((int32_t)(ticks - next) < 0) __asm volatile ("wfi");
        next += 1000;
        uint8_t b[2]; uint16_t vref = adc_read(13); uint16_t vdd = (uint16_t)(3000u * VREF_CAL / vref);
        int16_t die = die_temp_cC(adc_read(12), vdd);
        int16_t sens = ntc_cC(adc_read(0));                          /* NTC on PA0 */
        int ok = i2c_read(0x40, 0x04, b, 2);                         /* INA219 current register */
        uint16_t cur = ok ? (uint16_t)((b[0] << 8) | b[1]) : 0;
        uart_puts("t="); uart_int(sens); uart_puts(" d="); uart_int(die); uart_puts(" i="); uart_int(cur);
#ifdef USE_SENTINEL
        sn_sample_t s = { die, sens, cur, vdd, (uint8_t)ok, 1, (uint8_t)ok };
        uint8_t h = sn_step(&s);
        IWDG_KR = 0xAAAA;                                            /* still alive */
        if (!base_saved) { sn_baseline_t bl; if (sn_baseline_get(&bl)) { flash_save(&bl, sizeof bl); base_saved = 1; } }
        sn_memory_check_step();
        if (sn_bus_recover_requested()) { I2C1_CR1 = 0; I2C1_CR1 = 1; }   /* peripheral reset */
        uart_puts(" h=0x"); uart_hex8(h);
        if ((next / 1000) % 60 == 0) { sn_digest_t d; sn_digest(&d, 7); for (unsigned k = 0; k < sizeof d; k++) uart_hex8(((uint8_t *)&d)[k]); }
#endif
        uart_puts("\r\n");
    }
}

/* startup */
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
void Reset_Handler(void) {
    uint32_t *s = &_sidata, *d = &_sdata; while (d < &_edata) *d++ = *s++;
    for (d = &_sbss; d < &_ebss; ) *d++ = 0;
    main();
}
void Default_Handler(void) { for (;;); }
__attribute__((section(".isr_vector"), used)) void (*const vectors[16 + 32])(void) = {
    (void (*)(void))&_estack, Reset_Handler, Default_Handler, Default_Handler,
    [11] = Default_Handler, [14] = Default_Handler, [15] = SysTick_Handler,
};
