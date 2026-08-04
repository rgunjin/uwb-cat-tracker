# DW1000 notes

Open questions from an earlier bare-metal implementation
(`rgunjin/ss_twr_init_poll`), plus findings from the Zephyr in-tree
driver. Unverified against this project — treat as leads, not facts.

## Open issue
Initiator transmits (TXFRS set), responder never detects SFD.
Root cause not found.

## 1. AON wake-up
Reset first (`WCFG=0`, `CFG0=0`), only then save the config.
Order matters — save-before-reset leaves the chip in a bad state.

## 2. Mutex
`decamutexon()` / `decamutexoff()` are no-ops in the Decawave
reference code. Not a source of problems.

## 3. Preamble codes must match
Initiator and responder had different codes (9 vs 10).
Set both to 10:
    CFG_TX_CODE = 10
    CFG_RX_CODE = 10
**Never tested** — found late, not applied.

## 4. LDE_REPC for code 10
    CFG_LDE_RESP = 0x3335   // preamble codes 9 and 10, 64 MHz PRF
Depends on preamble code — must be updated together with (3).

## 5. TX_CTRL
Align with the Decawave reference init sequence:
    dwt_initialise()
    dwt_configure()
    dwt_settxantennadelay()
    dwt_setrxantennadelay()
    dwt_setcallbacks()
    dwt_setinterrupt()
    dwt_setrxaftertx()
    dwt_setrxtimeout()

## Reference: Zephyr in-tree driver

Zephyr ships an IEEE 802.15.4 driver for this chip:
`drivers/ieee802154/ieee802154_dw1000.c`. It targets packet radio, not
ranging — no `dwt_*` API, no delayed TX, no antenna delay calibration —
so it cannot be used for TWR directly.

It is, however, working, maintained code for exactly this board, and it
answers several of the questions above:

- `dwt_hw_reset()` — hardware reset sequence and required delays
- `dwt_reset_rfrx()` — receiver reset; the comment references
  DW1000 User Manual §4.1.6 *RX Message timestamp*, noting that a
  receiver reset is required after reception or timestamps are wrong.
  Directly relevant to TWR.
- SPI configuration is held in the runtime context rather than a const
  struct, which suggests the clock rate is changed at runtime — the chip
  requires a low rate before PLL lock and after wake-up. Relevant to
  item 1 above.

The devicetree node already exists in
`boards/qorvo/decawave_dwm1001_dev/decawave_dwm1001_dev.dts`:

    ieee802154: dw1000@0 {
        compatible = "decawave,dw1000";
        ...
    };

This describes the hardware, not a driver binding, so the same node is
reused by the ranging implementation. `CONFIG_IEEE802154_DW1000` must
stay disabled, otherwise the in-tree driver claims the device and both
implementations contend for the SPI bus.
