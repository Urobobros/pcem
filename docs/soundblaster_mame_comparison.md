# Sound Blaster Emulation: PCem vs. MAME

The MAME project provides a reference implementation for numerous sound devices. For Sound Blaster cards, MAME primarily handles register-level emulation and volume mixing via the `mixer_set` routine. Bass and treble registers simply scale the output without applying any digital filtering.

PCem implements additional filtering logic to simulate the original analog circuitry present on many cards. Functions like `low_iir` and `high_iir` in `includes/private/filters.h` apply second‑order IIR filters during mixing to approximate bass and treble adjustments.

While MAME focuses on accurate hardware behaviour, PCem's approach provides more perceptible bass and treble effects at the cost of extra CPU time. These filters can be found in `sound_sb.c` and `filters.h`.

PCem now includes an optional "Surround" effect controlled by mixer register `0x48` (or `0x90` when toggled via Creative's CT3DSE utility). When enabled, it mixes a portion of each channel with opposite phase to widen the stereo image. MAME lacks this feature.

PCem exposes an AWE64 selection that maps to the same emulation core as its AWE32 implementation. By contrast, the x86Box emulator contains a separate AWE64 model with more accurate mixer behaviour.
