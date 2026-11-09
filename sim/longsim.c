/* 30-day runs at 1 sample/s through the real sentinel.c: daily cycle, slow season, weather fronts,
   radio bursts. Normal runs count false alarms; drift runs add a slow sensor offset. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "sentinel.h"
const uint8_t __app_start[4], __app_end[1]; uint32_t __stack_canary = 0xC0DEFACEu;
static uint64_t st = 88172645463325252ull;
static double U(void) { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return (st >> 11) * (1.0 / 9007199254740992.0); }
static double G(void) { double u = U() + 1e-12, v = U(); return sqrt(-2 * log(u)) * cos(6.283185307 * v); }
#define DAY 86400L
int main(int argc, char **argv) {
    double rate = atof(argv[1]); int days = atoi(argv[2]); st += strtoull(argv[3], 0, 10) * 7919;
    long T = days * DAY, onset = rate > 0 ? (long)(DAY * (2 + 3 * U())) : T;
    double base = 22 + 10 * U(), season = -4 + 8 * U(), diurnal = 3 + 3 * U(), ph = 6.28 * U();
    double w = 0, front = 0, front_t = 0, die = 0, sen = 0, selfheat = 0;
    long first_fa = -1, det = -1; int fa_flag = 0;
    sn_init();
    for (long t = 0; t < T; t++) {
        if (U() < 1.0 / (3 * DAY)) front_t = -3 + 6 * U();                 /* a front every ~3 days */
        front += (front_t - front) / 1800.0;                              /* arrives over ~30 min */
        w += G() * 0.0015; w *= 0.99999;
        double env = base + season * t / T + diurnal * sin(6.283185307 * t / DAY + ph) + w + front;
        if (t == 0) { die = env; sen = env; }                             /* power-on: chip starts at ambient */
        selfheat += (6 - selfheat) / 120.0;                               /* and warms itself over a few minutes */
        die += (env + selfheat - die) / 20; sen += (env - sen) / 40;
        double d = die + G() * 0.4, s = sen + G() * 0.06, i = 12 + G() * 0.1;
        if (U() < 0.1) i += 18;
        if (t >= onset) s += rate * (t - onset) / (double)DAY;
        sn_sample_t smp = { (int16_t)lround(d * 100), (int16_t)lround(s * 100), (uint16_t)lround(i * 100), 3300, 1, 1, 1 };
        uint8_t h = sn_step(&smp);
        if ((h & 31) && t < onset && first_fa < 0) { first_fa = t; fa_flag = h & 31; }
        if ((h & 2) && t >= onset && det < 0) { det = t; if (rate > 0) break; }
    }
    printf("%ld %d %ld %ld\n", first_fa, fa_flag, onset, det);
}
