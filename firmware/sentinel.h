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
    uint8_t  bus_ok;     /* 1 = every I2C transaction this sample ACKed and completed */
    uint8_t  sens_ok;    /* external sensor value is valid */
    uint8_t  cur_ok;     /* current value is valid (INA219 read succeeded) */
} sn_sample_t;

/* learned baseline, saved to flash by the app so a reset during a fault
   does not re-learn the fault as "normal" */
typedef struct {
    int16_t res_mu, res_sd, cur_mu, cur_sd;
    int32_t dvar_base, die_slow;
    uint16_t magic, crc;
} sn_baseline_t;

/* reset causes the app passes in at boot (reported in the digest) */
#define RST_WATCHDOG 0x40u
#define RST_POWER    0x80u

/* 16-byte digest sent to the fleet gateway */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint8_t  health;          /* current health byte */
    uint8_t  seen;            /* bits 0-5: flags seen since last report, 6: watchdog reset, 7: power reset */
    uint32_t uptime_s;
    uint16_t last_fault_min;  /* minutes since last confirmed fault, 0xFFFF = none */
    int16_t  sens_cC;         /* latest sensor value, 0.01 degC */
    int16_t  resid_cC;        /* (sensor - chip) minus learned normal: lets the gateway trend drift */
    uint16_t crc;             /* CRC-16 over the first 14 bytes */
} sn_digest_t;

void    sn_init(void);
uint8_t sn_step(const sn_sample_t *s);       /* call once per sample, returns health byte */
uint8_t sn_bus_recover_requested(void);       /* app performs I2C bus reset when 1 */
void    sn_digest(sn_digest_t *d, uint16_t node_id);
uint8_t sn_baseline_get(sn_baseline_t *b);       /* 1 if a learned baseline exists */
uint8_t sn_baseline_set(const sn_baseline_t *b); /* 1 if accepted (magic + CRC ok) */
void    sn_note_reset(uint8_t rst_bits);
uint8_t sn_memory_check_step(void);           /* incremental flash CRC + stack canary */
#endif
