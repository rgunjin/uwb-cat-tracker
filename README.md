# uwb-cat-tracker

Real-time position tracking of a cat in a courtyard using Ultra-Wideband (UWB)
two-way ranging.

**Status:** two nodes measure the distance between them. Accuracy is not
calibrated yet.

## Hardware

- 3× Qorvo DWM1001 (Nordic nRF52832 + Decawave DW1000) as fixed anchors
- 1× DWM1001 as a mobile tag worn by the cat
- Board target: `decawave_dwm1001_dev`

## Software

- Zephyr RTOS 4.4.0
- Zephyr SDK 1.0.1 (`arm-zephyr-eabi`, GCC 14.3)
- Qorvo's DW1000 driver, vendored under `vendor/decadriver/`
- Position computation in Python on a host PC

## Structure

    src/main.c          init sequence and role dispatch
    src/deca_port.c     SPI, GPIO and reset hooks the vendor driver calls
    src/uwb_radio.c     frame transport: send, receive, timestamps
    src/uwb_msg.h       on-air message format
    src/initiator.c     sends the poll, computes the distance
    src/responder.c     replies with its own timestamps
    vendor/decadriver/  Qorvo's driver, unmodified except as noted in its README

The node role is chosen at build time through Kconfig, not at runtime, so
the two roles are two separate builds.

## Build

Requires a Zephyr workspace at v4.4.0 with `ZEPHYR_BASE` set:

    west build -b decawave_dwm1001_dev -d build_initiator -- -DEXTRA_CONF_FILE=initiator.conf
    west build -b decawave_dwm1001_dev -d build_responder -- -DEXTRA_CONF_FILE=responder.conf

    west flash -d build_initiator

Add `info.conf` to print the module's OTP calibration at startup:

    -DEXTRA_CONF_FILE="initiator.conf;info.conf"

Serial output at 115200 baud:

    picocom -b 115200 /dev/ttyACM0

## Roadmap

- [x] Out-of-tree Zephyr application skeleton
- [x] DW1000 driver port (SPI, GPIO, delay hooks)
- [x] Frame exchange between two nodes
- [x] Single-sided two-way ranging
- [ ] Antenna delay calibration
- [ ] Asymmetric double-sided two-way ranging
- [ ] Three anchors, ranging data collected on a central node
- [ ] Trilateration and web dashboard

## Notes

`docs/config.md` — why each radio parameter is what it is: six of them are
fixed by the module's regulatory certification, the rest follow from the
User Manual.

`docs/modules.md` — the nine modules by label, part ID and factory
calibration.

`docs/dw1000-notes.md` — findings carried over from an earlier bare-metal
implementation ([rgunjin/ss_twr_init_poll](https://github.com/rgunjin/ss_twr_init_poll)).

Commit messages follow a hypothesis-driven template (`.gitmessage`): each
change records what was expected, what was observed, and whether the
hypothesis held.
