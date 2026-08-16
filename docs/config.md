# Configuration rationale

All radio parameters below are fixed by the DWM1001C datasheet §10:
the module's regulatory certification is only valid for this exact
configuration. They are not tuning knobs.

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

### rxPAC / sfdTO
[заполнить, оба производные от PLEN_128]

### Smart TX Power
Enabled. Receiver sensitivity −92 dBm → −98 dBm.

## Per-device calibration

### XTAL_TRIM — from OTP, per unit
Measured spread across 9 modules: 15…25 (~15 ppm before trimming).
Not applied automatically; dwt_initialise() writes it to FS_XTALT.

### Antenna delay — 16472, from OTP
The vendor port layer hardcodes 16456, deliberately low so the residual
error is positive. This project uses the per-device OTP value.
Known drift: 2.15 mm/°C, 5.35 cm/V (UM §8.3, p. 204).

## Ranging scheme

### Asymmetric DS-TWR (UM §12.3.4.3)
Clock-offset error ~2 mm versus tens of centimetres for SS-TWR, and
N+2 messages versus 3N for the symmetric variant with multiple anchors.
Tag is the initiator; anchors compute the distances.
SS-TWR is kept as a bring-up path: with modules trimmed to ~3 ppm it
gives roughly 10 cm, which is good enough to validate the chain.

## Known limitations
- Anchors are mounted in fourth-floor windows and are collinear.
  A collinear layout does not determine position uniquely; resolved by
  assuming the cat is at ground level and discarding the solution
  inside the building. These are assumptions, not measurements.
- TWR measures slant range; horizontal projection amplifies range error
  near the wall.
- DW1000 predates IEEE 802.15.4z and has no STS, so ranging can be
  spoofed. Not relevant here, but worth stating.
