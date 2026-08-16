# Decawave DW1000 device driver

Vendored from the Decawave DWM1001 SDK (`dwm1001-examples`),
directory `deca_driver/`.

Driver version: **04.00.06** (`DW1000_DRIVER_VERSION 0x040006`).
This appears to be the final DW1000 driver release — no newer version
is published, and Qorvo's development has moved to the DW3000 family.

## Files

| File                  | Purpose                                       |
|-----------------------|-----------------------------------------------|
| `deca_device.c`       | Device logic, the `dwt_*` API implementation  |
| `deca_device_api.h`   | Public API                                    |
| `deca_params_init.c`  | Configuration constant tables                 |
| `deca_param_types.h`  | Types for those tables                        |
| `deca_regs.h`         | Register and bit definitions                  |
| `deca_types.h`        | Integer type aliases                          |
| `deca_version.h`      | Driver version                                |
|-----------------------|-----------------------------------------------|

## Local changes

- `deca_types.h` — rewritten on top of `<stdint.h>`. The original
  defined `uint32` as `unsigned long`, which is 64-bit on a host
  build (`native_sim`). The original header also guarded its
  typedefs with `#ifndef uint8`, which never triggers, since these
  are typedefs rather than macros.

- `deca_device_api.h` — the duplicated typedef block (lines 18-55 in
  the original) replaced with `#include "deca_types.h"`. The header
  declared the same six types a second time; this only worked
  because of the `_DECA_UINT32_` style guards, which the rewrite
  above removed.

Everything else is unmodified. Any further change to vendored code
goes in this list.

## What is not here

The driver calls out to three platform hooks — `readfromspi()`,
`writetospi()` and `deca_sleep()` — which are implemented for Zephyr
in `src/deca_port.c`. The SDK ships its own `port/port_platform.c`
targeting the nRF5 SDK; it is not used here.

## License

See `LICENSE.txt` (the SDK's `disclaimer.txt`).
