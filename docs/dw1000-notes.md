# DW1000 findings (bare-metal, April 2026)

Notes carried over from the bare-metal implementation
(`~/dwm1001-lab/dwm1001_driver`). Not yet verified against the
Zephyr port — treat as leads, not facts.

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
