# Configuration rationale

The six RF parameters below — channel, data rate, PRF, preamble
length, preamble code and modulation — are fixed by the DWM1001C
datasheet §10: the module's regulatory certification is only valid
for this exact configuration. They are not tuning knobs. The
remaining fields (`rxPAC`, `sfdTO`, `nsSFD`, `phrMode`) follow from
those six and are derived from the User Manual.

## Radio

### Channel 5 (6489.6 MHz)

Fixed by certification. Independently confirmed by ETSI EN 302 065:
the 6.0–8.5 GHz band is unrestricted in Europe, while channels 1–3
(3.1–4.8 GHz) require LDC or DAA. Channel 4 was ruled out for this
reason.

### Data rate 6.81 Mbit/s

Wins on accuracy, energy per exchange and air time. Loses only on
range: ~60 m LOS versus ~250 m at 110 kbit/s. The deployment area
is a courtyard of roughly 30 m, so range is not the binding constraint.

### PRF 64 MHz

More energy in the preamble, better first-path detection.

### Preamble length 128

Fixed by certification. A shorter preamble (64) would reduce T_reply
and therefore clock-drift error — hypothesis REFUTED on regulatory
grounds, not technical ones.

### Preamble code 9

Fixed by certification. TX and RX codes must be identical: with
mismatched codes the receiver does not see a degraded signal, it sees
nothing. Changing the code also requires updating LDE_REPC (UM §7.2.47).

### rxPAC — DWT_PAC8

Preamble Acquisition Chunk: the number of preamble symbols the
receiver correlates as one unit. `deca_device_api.h` recommends PAC 8
for preamble lengths of 128 and below, PAC 16 for 256, PAC 32 for 512
and PAC 64 for 1024 and above.

### sfdTO — 129 symbols

SFD detection timeout (`DRX_SFDTOC`, UM §7.2.40.7). The counter starts
when preamble is detected; if no SFD arrives before it expires, the
reception is aborted and `RXSFDTO` is set in `SYS_STATUS`. Its purpose
is to recover from false preamble detections — without it the receiver
would stay on indefinitely, which the User Manual explicitly warns
against for battery-powered devices.

    sfdTO = preamble length + SFD length + 1 - PAC size
          = 128 + 8 + 1 - 8
          = 129

The PAC size is subtracted because one PAC worth of preamble is
consumed by the detection itself. The driver default,
`DWT_SFDTOC_DEF = 4161`, is 4096 + 64 + 1 — sized for the longest
possible preamble and SFD, and roughly thirty times larger than
necessary here.

### Standard SFD (nsSFD = 0)

UM Table 21 recommends the standard 8-symbol IEEE SFD at 6.8 Mbit/s:
`DWSFD`, `TNSSFD` and `RNSSFD` all cleared. `dwt_configure()` sets
these from `nsSFD`.

### Smart TX Power

Enabled. The transmitter may exceed the nominal power on short frames,
since the −41.3 dBm/MHz limit applies to the average over a
millisecond. The DWM1001 datasheet quotes an equivalent sensitivity of
−98 dBm with Smart TX Power against −92 dBm without.

The per-device calibration lives in OTP (channel 5, PRF 64 at address
0x019) as four boost levels — `dwt_txconfig_t` documents them as
≤0.125 ms, ≤0.25 ms, ≤0.5 ms and default. Across nine modules the
upper nibble of each byte was identical (2/4/6/8, the boost steps)
while the lower nibble varied per unit.

## Per-device calibration

### XTAL_TRIM — from OTP, per unit

The factory trim value lives in OTP at 0x01E, but the hardware does
not apply it on its own: on reset `FS_XTALT` comes up at a default.
`dwt_initialise()` reads OTP and writes the value into the register,
so the correction only takes effect once the driver has run.

Measured spread across 9 modules: 15…25, roughly 15 ppm before
trimming. Verified on hardware — a module reporting XTAL_TRIM 22 in
OTP reads back `FS_XTALT = 0x76`, whose lower five bits are 22.

### Antenna delay — 16472, from OTP

The vendor port layer hardcodes 16456, deliberately low so the residual
error is positive. This project uses the per-device OTP value.
Known drift: 2.15 mm/°C, 5.35 cm/V (UM §8.3, p. 204).

## Ranging scheme

### Asymmetric DS-TWR (UM §12.3.4.3)

Clock-offset error ~2 mm versus tens of centimetres for SS-TWR, and
N+2 mess
