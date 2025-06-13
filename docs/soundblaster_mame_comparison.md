# Sound Blaster Emulation: PCem vs. MAME

The MAME project provides a reference implementation for numerous sound devices. For Sound Blaster cards, MAME primarily handles register-level emulation and volume mixing via the `mixer_set` routine. Bass and treble registers simply scale the output without applying any digital filtering.

PCem implements additional filtering logic to simulate the original analog circuitry present on many cards. Functions like `low_iir` and `high_iir` in `includes/private/filters.h` apply second‑order IIR filters during mixing to approximate bass and treble adjustments.

While MAME focuses on accurate hardware behaviour, PCem's approach provides more perceptible bass and treble effects at the cost of extra CPU time. These filters can be found in `sound_sb.c` and `filters.h`.

PCem now includes an optional "Surround" effect controlled by mixer register `0x48`. When enabled, it mixes a portion of each channel with opposite phase to widen the stereo image. MAME lacks this feature.
