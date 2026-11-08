#include "sentinel.h"

#define LEARN_N      120   /* samples used to learn the healthy baseline */
#define CONFIRM_N      3   /* consecutive triggers to confirm */
#define CLEAR_N       10   /* clean samples to return to healthy */
#define DEGRADE_N     30   /* confirmed this long after recovery -> degraded */

typedef struct { int32_t m; int32_t v; } ew_t;        /* exp-weighted mean/var */
typedef struct { uint8_t run, clean, state; uint8_t age; } det_t;

static struct {
    uint16_t n;
    ew_t die_slow, die_lp, res, dres, cur; /* learned baselines */
    uint16_t cur_hist[2];
    ew_t dres_fast, cur_fast;
    int32_t cus_up, cus_dn;              /* CUSUM on residual sensor-die */
    int16_t prev_sens; uint8_t have_prev;
    int16_t res_mu, res_sd, cur_mu, cur_sd; int32_t dvar_base;
    det_t d[6];
    uint8_t bus_fail_run, recover_req;
    uint32_t t, last_fault; uint8_t latched; uint16_t vdd;
} S;

static inline void ew_upd(ew_t *e, int32_t x, uint8_t k) {
    int32_t d = x - e->m;
    if (d > 30000) d = 30000; else if (d < -30000) d = -30000;
    e->m += d >> k;
    e->v += ((d * d) >> k) - (e->v >> k);
}
static uint16_t isqrt32(uint32_t x) {          /* shift/subtract, no divide */
    uint32_t r = 0, b = 1uL << 30;
    while (b > x) b >>= 2;
    while (b) { if (x >= r + b) { x -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
    return (uint16_t)r;
}
static void det_feed(uint8_t i, uint8_t trig) {
    det_t *q = &S.d[i];
    if (trig) {
        q->clean = 0; if (q->run < 255) q->run++;
        if (q->state == ST_HEALTHY) q->state = ST_SUSPECT;
        if (q->run >= CONFIRM_N && q->state == ST_SUSPECT) { q->state = ST_CONFIRMED; q->age = 0; S.last_fault = S.t; }
        if (q->state == ST_CONFIRMED && ++q->age >= DEGRADE_N) q->state = ST_DEGRADED;
    } else {
        q->run = 0;
        if (q->state != ST_HEALTHY && ++q->clean >= CLEAR_N) { q->state = ST_HEALTHY; q->clean = 0; }
    }
}

void sn_init(void) { uint8_t *p = (uint8_t *)&S; for (unsigned i = 0; i < sizeof S; i++) p[i] = 0; }

uint8_t sn_step(const sn_sample_t *s) {
    uint8_t trig[6] = {0};
    S.t++; S.vdd = s->vdd_mV;
    /* die sensor is noisy and reacts faster than the external sensor: low-pass it so both
       see the environment with roughly the same lag (time constant matched on the bench) */
    if (S.n == 0) S.die_lp.m = s->die_cC;
    ew_upd(&S.die_lp, s->die_cC, 4);
    int32_t res = s->bus_ok ? (int32_t)s->sens_cC - S.die_lp.m : 0;
    /* min of last 3 current samples rejects short radio bursts */
    uint16_t cf = s->cur_cmA; if (S.cur_hist[0] < cf) cf = S.cur_hist[0]; if (S.cur_hist[1] < cf) cf = S.cur_hist[1];
    S.cur_hist[1] = S.cur_hist[0]; S.cur_hist[0] = s->cur_cmA;
    int32_t dsens = 0;
    if (s->bus_ok && S.have_prev) dsens = (int32_t)s->sens_cC - S.prev_sens;

    if (S.n < LEARN_N) {                                   /* learning the healthy baseline */
        uint8_t k = S.n < 16 ? 2 : 4;
        if (S.n == 0) { S.die_slow.m = s->die_cC; S.die_lp.m = s->die_cC; S.res.m = 0; S.cur.m = s->cur_cmA; S.cur_hist[0] = S.cur_hist[1] = s->cur_cmA; }
        ew_upd(&S.res, res, k); ew_upd(&S.dres, dsens, k);
        if (S.n == 3) S.cur.m = cf;                                     /* skip start-up samples */
        else if (S.n > 3 && (S.n < 16 || (int32_t)cf < S.cur.m + 300)) ew_upd(&S.cur, cf, k);  /* ignore >3 mA spikes */
        ew_upd(&S.die_slow, S.die_lp.m, 3);
        if (++S.n == LEARN_N) {
            S.res_mu = (int16_t)S.res.m; S.res_sd = isqrt32(S.res.v); if (S.res_sd < 25) S.res_sd = 25;
            S.cur_mu = (int16_t)S.cur.m; S.cur_sd = isqrt32(S.cur.v); if (S.cur_sd < 30) S.cur_sd = 30;
            S.dvar_base = S.dres.v < 400 ? 400 : S.dres.v;
            S.dres_fast.v = S.dvar_base; S.cur_fast.m = S.cur_mu;
        }
    } else {
        /* BUS: every failed transaction is a trigger */
        if (!s->bus_ok) { trig[3] = 1; S.have_prev = 0; if (S.bus_fail_run < 255) S.bus_fail_run++; }
        else S.bus_fail_run = 0;
        S.recover_req = (S.bus_fail_run >= CONFIRM_N);

        if (s->bus_ok) {
            /* NOISE: short-term variance of sample-to-sample change vs learned */
            ew_upd(&S.dres_fast, dsens, 3);
            trig[2] = S.dres_fast.v > (S.dvar_base << 3);

            /* DRIFT / THERMAL: residual sensor - die cancels the environment */
            int32_t z = res - S.res_mu, kk = 2 * S.res_sd, h = (int32_t)S.res_sd * 8;
            S.cus_up += z - kk; if (S.cus_up < 0) S.cus_up = 0;
            S.cus_dn += -z - kk; if (S.cus_dn < 0) S.cus_dn = 0;
            if (S.cus_up > 4 * h) S.cus_up = 4 * h;
            if (S.cus_dn > 4 * h) S.cus_dn = 4 * h;
            int32_t rise = S.die_lp.m - S.die_slow.m;            /* die vs its own slow trend */
            uint8_t noisy = trig[2] || S.d[2].state != ST_HEALTHY;
            if ((S.cus_dn > h && rise > 200) || S.die_lp.m > 8000) trig[0] = 1;  /* die heated, sensor did not follow */
            else if (!noisy && (S.cus_up > h || (S.cus_dn > h && rise < 100))) trig[1] = 1;  /* sensor moved alone */
            if (!trig[0]) ew_upd(&S.die_slow, S.die_lp.m, 8);
        } else if (s->die_cC > 8000) trig[0] = 1;

        /* POWER: sustained rail current shift (radio bursts are 1 sample, ignored) */
        int32_t lim = 4 * S.cur_sd; if (lim < 150) lim = 150;
        int32_t cx = (int32_t)cf - S.cur_mu;                      /* clip single outliers */
        if (cx > 2 * lim) cx = 2 * lim; else if (cx < -2 * lim) cx = -2 * lim;
        ew_upd(&S.cur_fast, S.cur_mu + cx, 3);
        int32_t dc = S.cur_fast.m - S.cur_mu;
        trig[4] = dc > lim || dc < -lim;
        if (s->vdd_mV < 2900) trig[4] = 1;
    }
    if (s->bus_ok) { S.prev_sens = s->sens_cC; S.have_prev = 1; }

    uint8_t flags = 0, st = ST_HEALTHY;
    for (uint8_t i = 0; i < 6; i++) {
        det_feed(i, trig[i]);
        if (S.d[i].state >= ST_CONFIRMED) flags |= (uint8_t)(1u << i);
        if (S.d[i].state > st) st = S.d[i].state;
    }
    S.latched |= flags;
    return (uint8_t)(flags | (st << 6));
}

uint8_t sn_bus_recover_requested(void) { return S.recover_req; }

/* ---- memory health: CRC over the application image, one 64-byte chunk per call ---- */
extern const uint8_t __app_start[], __app_end[];
extern uint32_t __stack_canary;
static uint16_t crc16_step(uint16_t c, uint8_t b) {
    c ^= (uint16_t)b << 8;
    for (uint8_t i = 0; i < 8; i++) c = (c & 0x8000u) ? (uint16_t)((c << 1) ^ 0x1021u) : (uint16_t)(c << 1);
    return c;
}
static uint16_t mem_crc = 0xFFFF, mem_ref; static const uint8_t *mem_p; static uint8_t mem_ref_ok;
uint8_t sn_memory_check_step(void) {
    uint8_t bad = (__stack_canary != 0xC0DEFACEuL);
    if (!mem_p) mem_p = __app_start;
    for (uint8_t i = 0; i < 64 && mem_p < __app_end; i++) mem_crc = crc16_step(mem_crc, *mem_p++);
    if (mem_p >= __app_end) {
        if (!mem_ref_ok) { mem_ref = mem_crc; mem_ref_ok = 1; }
        else if (mem_crc != mem_ref) bad = 1;
        mem_crc = 0xFFFF; mem_p = __app_start;
    }
    det_feed(5, bad);
    return bad;
}

void sn_digest(sn_digest_t *d, uint16_t node_id) {
    uint8_t st = 0, fl = 0;
    for (uint8_t i = 0; i < 6; i++) { if (S.d[i].state >= ST_CONFIRMED) fl |= 1u << i; if (S.d[i].state > st) st = S.d[i].state; }
    d->node_id = node_id; d->health = (uint8_t)(fl | (st << 6)); d->latched = S.latched;
    d->uptime_s = S.t; d->last_fault_s = S.last_fault;
    d->die_C = (int8_t)(S.die_slow.m / 100); d->vdd_div16 = (uint8_t)(S.vdd >> 4);
    uint16_t c = 0xFFFF; const uint8_t *p = (const uint8_t *)d;
    for (uint8_t i = 0; i < 14; i++) c = crc16_step(c, p[i]);
    d->crc = c; S.latched = 0;
}
