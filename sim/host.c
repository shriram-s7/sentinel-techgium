#include <stdio.h>
#include "sentinel.h"
const uint8_t __app_start[4], __app_end[1]; uint32_t __stack_canary = 0xC0DEFACEu;
int main(void) {
    int die, sens, cur, vdd, bus, sok, cok; char line[128];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] == 'R') { sn_init(); puts("R"); continue; }
        sscanf(line, "%d %d %d %d %d %d %d", &die, &sens, &cur, &vdd, &bus, &sok, &cok);
        sn_sample_t s = { (int16_t)die, (int16_t)sens, (uint16_t)cur, (uint16_t)vdd, (uint8_t)bus, (uint8_t)sok, (uint8_t)cok };
        printf("%u\n", sn_step(&s));
    }
}
