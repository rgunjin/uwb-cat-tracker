# Module inventory

Nine DWM1001 modules, all from lot 0x04325100. Labels are written on
the module; PART_ID is read from OTP at 0x006 and printed at startup.

| Label | PART_ID    | XTAL_TRIM | TX_POWER (ch5, PRF64) | ANT_DELAY |
|-------|------------|-----------|-----------------------|-----------|
| T1    | 0xD4030E17 | 22        | 0x29496989            | reference |
| A1    | 0xD483490F | 22        | 0x2C4C6C8C            | 16451     |
| A2    | 0xD4834E05 | 22        | 0x2B4B6B8B            | —         |
| A3    | 0xD4820512 | 20        | 0x2B4B6B8B            | —         |
| S1    | 0xD4924E20 | 20        | 0x28486888            | —         |
| S2    | 0xD482CB1D | 23        | 0x2A4A6A8A            | —         |
| S3    | 0xD4841834 | 24        | —                     | —         |
| S4    |            |           |                       |           |
| S5    |            |           |                       |           |

OTP reports 16472 on every unit, but the real delay differs per
module. Only the *sum* of two delays is observable in a single
ranging exchange, so calibration is always against a partner.

T1 is the reference. Each anchor is calibrated against it, which
matches how the system runs: every exchange is tag to anchor, never
anchor to anchor. The tag's own error then appears as a common offset
across all anchors rather than as a distortion of the geometry.

The 16451 for A1 comes from a series at 1, 2, 3, 4 and 5 m against T1.
The error was constant at about -407 mm with no scale factor and no
non-linearity; one millimetre of range error is 0.107 units, since the
delay enters the result four times and T_prop is halved. A verification
run at 2 m afterwards read 1930 mm, 70 mm short — inside the spread of
a single window.

An earlier pair needed 16495, a difference of 44 units or roughly
410 mm, which is why the factory value alone is not enough.

The classic method in UM 8.3 solves three pairs A-B, A-C, B-C to get
per-module values: d_A = (err_AB + err_AC - err_BC) / 2. That was not
used here — three series instead of one, and no practical gain given
that only tag-to-anchor pairs ever occur.
