/* Sentinel - on-device health monitor for small MCUs.
 * Integer only, no heap, no divide in the per-sample path (Cortex-M0+ has no HW divider).
 * Units: temperatures in 0.01 degC, current in 0.01 mA, supply in mV. */
#ifndef SENTINEL_H
#define SENTINEL_H
#include <stdint.h>

/* health byte: bits 0-5 fault flags, bits 6-7 overall state */
#define H_THERMAL  0x01u
#define H_DRIFT    0x02u
#define H_NOISE    0x04u
#define H_BUS      0x08u
#define H_POWER    0x10u
#define H_MEMORY   0x20u
#define ST_HEALTHY   0u
#define ST_SUSPECT   1u
#define ST_CONFIRMED 2u
#define ST_DEGRADED  3u

typedef struct {
    int16_t  die_cC;     /* internal die temperature */
    int16_t  sens_cC;    /* external sensor value (valid only if bus_ok) */
    uint16_t cur_cmA;    /* rail current from INA219 */
    uint16_t vdd_mV;     /* from VREFINT */
    uint8_t  bus_ok;     /* 1 = I2C transaction ACKed and completed */
} sn_sample_t;

/* 16-byte digest sent to the fleet gateway */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint8_t  health;        /* current health byte */
    uint8_t  latched;       /* every flag seen since last report */
    uint32_t uptime_s;
    uint32_t last_fault_s;
    int8_t   die_C;
    uint8_t  vdd_div16;
    uint16_t crc;
} sn_digest_t;

void    sn_init(void);
uint8_t sn_step(const sn_sample_t *s);       /* call once per sample, returns health byte */
uint8_t sn_bus_recover_requested(void);       /* app performs I2C bus reset when 1 */
void    sn_digest(sn_digest_t *d, uint16_t node_id);
uint8_t sn_memory_check_step(void);           /* incremental flash CRC + stack canary */
#endif
