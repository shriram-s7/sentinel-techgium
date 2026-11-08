# Sentinel: self-diagnosing edge nodes

TECHgium 2026 (10th edition), Challenge 40: Self-Diagnosing Edge Devices. PID TG0922825.

A small firmware layer for low-end MCUs (STM32G0, Cortex-M0+). Each sample (1 Hz) it checks the
board, the external sensor, the I²C bus, the supply current and its own flash, and returns a
1-byte health code that travels with every reading. It needs no cloud connection.

## What is in here

| Folder | Contents |
|---|---|
| `firmware/` | `sentinel.c/.h` (the self-diagnosis module), `main.c` (minimal STM32G030 sensor node used for the A/B footprint build), `g030.ld` |
| `sim/` | `host.c` runs the same `sentinel.c` on a PC; `sim.py` generates fault and normal runs and scores them |
| `tools/` | `emu.py` counts instructions per call by running the compiled ARM code in Unicorn |
| `docs/` | presentation deck and figures |

## Health byte

| bit | 7..6 | 5 | 4 | 3 | 2 | 1 | 0 |
|---|---|---|---|---|---|---|---|
| meaning | state (00 healthy, 01 suspect, 10 confirmed, 11 degraded) | memory | power | bus | noise | drift | heat |

## Build and reproduce

```bash
cd firmware && make          # needs arm-none-eabi-gcc; prints size of base vs diag build
cd ../sim && make run        # needs gcc + python3; 30 runs per fault type, seed in sim.py
cd ../tools && pip install unicorn pyelftools && python3 emu.py
```

## Current results (simulation, not hardware yet)

Measured with arm-none-eabi-gcc 13.2, `-Os`, and a fresh simulation seed (777) not used for tuning.

| | Value |
|---|---|
| Flash added | 1,736 B (diag build minus base build) |
| RAM added | 144 B static + 56 B peak stack |
| Instructions per second | about 6.6k (sn_step ≈ 545, memory CRC chunk ≈ 6,000) |
| Heat / noise / bus / drift / power caught | 30 / 27 / 30 / 30 / 29 out of 30 |
| False alarms | 0 in 30 normal runs and in the time before every fault (about 18 h) |

Not yet done: hardware runs, current measurement with diagnostics on vs off, flash event log,
fallback-sensor and reduced-sampling recovery actions.
