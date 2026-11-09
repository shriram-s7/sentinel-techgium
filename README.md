# Sentinel: self-diagnosing edge nodes

TECHgium 2026 (10th edition), Challenge 40: Self-Diagnosing Edge Devices. PID TG0922825.

A small firmware layer for low-end MCUs (STM32G0, Cortex-M0+). Every sample (1 Hz) it checks the
board temperature against an external NTC sensor, the I²C bus, the MCU's supply current and its own
flash, and returns a 1-byte health code that travels with every reading. No cloud needed.

## Layout

| Folder | Contents |
|---|---|
| `firmware/` | `sentinel.c/.h` (self-diagnosis), `main.c` (STM32G030 node: NTC on ADC, INA219 on I²C, watchdog, baseline saved to flash), `g030.ld` |
| `sim/` | `host.c` / `longsim.c` run the same `sentinel.c` on a PC; `sim.py` (short fault runs), `ablation.py`, `longrun.py` (30-day runs) |
| `tools/` | `emu.py` counts instructions per call by running the compiled ARM code in Unicorn |
| `docs/` | presentation deck and figures |

## Health byte

| bit | 7..6 | 5 | 4 | 3 | 2 | 1 | 0 |
|---|---|---|---|---|---|---|---|
| meaning | state (00 healthy, 01 suspect, 10 confirmed, 11 degraded) | memory | power | bus | noise | drift | heat |

## Reproduce

```bash
cd firmware && make                     # arm-none-eabi-gcc; size of base vs diag build
cd ../sim && make run                   # gcc + python3
cd ../tools && pip install unicorn pyelftools && python3 emu.py
```

## Results so far (simulation, not hardware yet)

Footprint, arm-none-eabi-gcc 13.2 `-Os`, diag build minus base build:

| | Value |
|---|---|
| Flash | +2,384 B code (7.3% of 32 KB); the 24 B baseline is stored in one flash page |
| RAM | +164 B static, about +96 B stack at peak (3.2% of 8 KB) |
| CPU | about 6.6k instructions per second (estimate; to be timed with a GPIO toggle) |

Short runs (seed 2027, not used for tuning), 30 per type, first flag raised:

| Injected | Caught | First flag correct | Median time |
|---|---|---|---|
| Heat | 30/30 | 28 (2 first flagged as drift) | 27 s |
| Drift | 30/30 | 30 | 80 s |
| Noise | 30/30 | 30 | 8 s |
| Bus (hard or 12–62% intermittent) | 30/30 | 30 | 2 s |
| Power | 28/30 | 28 (missed +1.0 mA steps) | 9 s |
| Normal | 0 false alarms | | |

30-day runs (100 normal, 60 slow drift at 0.05–0.5 °C/day): every drift flagged once the sensor was
about 0.6 °C off (worst 0.73 °C); 4 false drift alarms in 3,205 node-days.

Not done yet: hardware runs, current measurement (bench meter), flash event log, the non-bus
recovery actions.
