# Testing Creative 3D Stereo Enhancement

PCem emulates the CT1745 mixer used on Sound Blaster 16 cards. Creative's CT3DSE utility toggles the 3D Stereo effect by writing to mixer register `0x90`.

To verify the feature in DOS:

1. Boot a DOS VM with a Sound Blaster 16 selected.
2. Run `CT3DSE.EXE` and choose **ON**.
3. Reading mixer register `0x90` should return `01`, and the emulator will apply a simple phase-inverted mix similar to the real hardware.
4. Choose **OFF** to disable the effect (register returns `00`).

If the utility reports no mixer, ensure the correct sound card is configured. 86Box implements the same register for compatibility.
