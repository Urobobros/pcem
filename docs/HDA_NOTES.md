# Intel HDA Implementation Notes

This document tracks the ongoing work on the Intel High Definition Audio device ported from QEMU.

## Current Status
- The device registers on the PCI bus and claims standard Intel IDs so guest drivers initialise.
- A 16&nbsp;KiB MMIO region is exposed via BAR0 so the guest can map control registers.
- A very small set of MMIO registers is implemented.  A simple Buffer Descriptor
  List (BDL) engine fetches PCM samples from guest memory so real audio plays
  back.  Basic interrupts are generated on buffer completion so streaming can
  continue.  Two independent playback engines allow mixing streams while a
  second descriptor engine captures silence into guest memory to exercise recording paths.  A minimal CORB/RIRB handler lets the guest
  query codec parameters such as the vendor ID.  Basic power state transitions
  are honoured so the guest can reset and wake the device.

## Next Steps
1. ~~Allocate a BAR-mapped register region so guest drivers can access control registers.~~
2. ~~Implement a simple Buffer Descriptor List (BDL) reader to fetch audio samples from guest memory.~~
3. ~~Generate interrupts on buffer completion to let the guest continue streaming.~~
4. ~~Expand to support capture streams and additional mixer registers.~~

## Follow-up Tasks
The groundwork is now in place for a functional Intel HDA device.  The
following items would bring the implementation closer to QEMU's model:

5. ~~Provide DMA position registers so guests can poll playback progress
   instead of relying solely on interrupts.~~
6. ~~Flesh out the codec emulation and implement a handful of verbs to
   negotiate sample rate, channel count and mixer settings.~~
7. ~~Feed captured audio from the host microphone into guest buffers
   rather than recording silence.~~
8. ~~Implement the remaining global control and status registers such as
   reset and wake so operating systems recognise power transitions.~~

These notes should help continue the work without losing track of what remains.

## Future Improvements
Even with microphone input and playback working, the experimental HDA device is far from feature complete.  The following larger tasks would bring the emulation closer to the QEMU implementation:

9. ~~Support multiple PCM streams so guest operating systems can mix voices and perform full duplex playback.~~
10. ~~Emulate additional codec nodes (pin widgets, DACs, mixers) and implement a broader set of verbs.~~
11. ~~Hook up the beep generator and memory based descriptor ring so system beeps and DMA offload work correctly.~~
12. ~~Implement power state transitions and wake events to better match real hardware behaviour.~~

