#ifndef _SOUND_HDA_H_
#define _SOUND_HDA_H_

#include <pcem/devices.h>

extern device_t hda_device;

/* Simple verb definitions used by the experimental codec */
#define HDA_VERB_GET_PARAMETER    0x0f00
#define HDA_PAR_VENDOR_ID         0x00

/* Power state definitions */
#define HDA_STATE_D0              0
#define HDA_STATE_D3              3

#endif /* _SOUND_HDA_H_ */
