# uwb-cat-tracker

Real-time position tracking of a cat in a courtyard using Ultra-Wideband (UWB)
two-way ranging.

**Status:** work in progress — environment and build system set up, DW1000
driver port next.

## Hardware

- 3× Qorvo DWM1001 (Nordic nRF52832 + Decawave DW1000) as fixed anchors
- 1× DWM1001 as a mobile tag worn by the cat
- Board target: `decawave_dwm1001_dev`

## Software

- Zephyr RTOS 4.4.0
- Zephyr SDK 1.0.1 (`arm-zephyr-eabi`, GCC 14.3)
- Position computation in Python on a host PC

## Build

Requires a Zephyr workspace at v4.4.0 with `ZEPHYR_BASE` set:

    west build -b decawave_dwm1001_dev
    west flash

Serial output at 115200 baud:

    picocom -b 115200 /dev/ttyACM0

## Roadmap

- [x] Out-of-tree Zephyr application skeleton
- [ ] DW1000 driver port (SPI, GPIO, delay hooks)
- [ ] Single-sided two-way ranging between two nodes
- [ ] Three anchors, ranging data collected on a central node
- [ ] Trilateration and web dashboard

## Notes

`docs/dw1000-notes.md` — findings carried over from an earlier bare-metal
implementation ([rgunjin/ss_twr_init_poll](https://github.com/rgunjin/ss_twr_init_poll)).

Commit messages follow a hypothesis-driven template (`.gitmessage`): each
change records what was expected, what was observed, and whether the
hypothesis held.
