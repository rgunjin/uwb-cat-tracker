# Module inventory

Nine DWM1001 modules, all from lot 0x04325100. Labels are written on
the module; PART_ID is read from OTP at 0x006 and printed at startup.
The J-Link serial number is the on-board debugger's, not the DW1000's,
and is what selects a board when more than one is plugged in.

| Label | PART_ID    | J-Link S/N | XTAL_TRIM | ANT_DELAY |
|-------|------------|------------|-----------|-----------|
| T1    | 0xD4030E17 | 760042200  | 22        | reference |
| A1    | 0xD483490F | 760042162  | 22        | 16430     |
| A2    | 0xD4834E05 |            | 22        | —         |
| A3    | 0xD482CB1D |            | 23        | —         |
| S1    | 0xD4924E20 |            | 20        | —         |
| S2    | 0xD4820512 |            | 20        | —         |
| S3    | 0xD4841834 |            | 24        | —         |
| S4    |            |            |           |           |
| S5    |            |            |           |           |

With two or more boards connected, every tool that talks over SWD
needs the serial number or it opens a graphical picker and waits:

    west flash -d build_initiator --dev-id 760042200
    JLinkRTTLogger -SelectEmuBySN 760042200 ...

Without it the choice is not even stable — two consecutive runs of
`JLinkExe` picked different boards.

TX_POWER used to be listed here. It is per-unit, but the firmware
reads it from OTP at 0x019 and writes it to the chip itself, so the
table was a copy of something nothing reads. What it was evidence for
is recorded in `docs/config.md` instead: across the nine modules the
upper nibble of each byte was identical and the lower one varied,
which is what makes it a factory calibration rather than a constant.

XTAL_TRIM is applied from OTP the same way, by `dwt_initialise()`.
It stays in the table because the spread across units — 15 to 25,
roughly 15 ppm — is the reason clock offset has to be corrected at
runtime at all.

OTP reports an antenna delay of 16472 on every unit, but the real
delay differs per module. Only the *sum* of two delays is observable
in a single ranging exchange, so calibration is always against a
partner.

T1 is the reference. Each anchor is calibrated against it, which
matches how the system runs: every exchange is tag to anchor, never
anchor to anchor. The tag's own error then appears as a common offset
across all anchors rather than as a distortion of the geometry.

The 16451 for A1 came from a series at 1, 2, 3, 4 and 5 m against T1:
the error was constant at about -407 mm with no scale factor and no
non-linearity, and one millimetre of range error is 0.107 units, since
the delay enters the result four times and T_prop is halved.

The value in the table is 16430, arrived at later. The series above
was measured with both boards on stools in a room five metres across,
where a floor reflection arrives within one CIR sample of the direct
path and drags the first-path timestamp late — the error grew with
distance for that reason, not because the delay was wrong. Two runs on
16430 in clean geometry read +200 mm at 5.10 m with a ratio of 3800
and +25 mm at 0.42 m with a ratio of 660, which puts the remaining
offset at about two units, below the spread of a single window.

16430 is therefore provisional: it is close, but it has never been
measured where the direct and reflected paths are properly separated.
Doing that needs the yard, not the room — see docs/config.md.
