#ifndef _SOUND_HDA_H_
#define _SOUND_HDA_H_

#include <pcem/devices.h>

extern device_t hda_device;

/* Simple verb definitions used by the experimental codec */
#define HDA_VERB_GET_PARAMETER           0x0f00
#define HDA_VERB_GET_CONN_LIST           0x0f02
#define HDA_VERB_GET_POWER_STATE         0x0f05
#define HDA_VERB_SET_POWER_STATE         0x0705
#define HDA_VERB_GET_PIN_WIDGET_CONTROL  0x0f07
#define HDA_VERB_SET_PIN_WIDGET_CONTROL  0x0707
#define HDA_VERB_GET_PIN_SENSE           0x0f09
#define HDA_VERB_GET_CONFIG_DEFAULT      0x0f1c

#define HDA_VERB_GET_STREAM_FORMAT       0x0a00
#define HDA_VERB_SET_STREAM_FORMAT       0x0200
#define HDA_VERB_SET_CHANNEL_STREAMID    0x0706
#define HDA_VERB_GET_CONV                0x0f06
#define HDA_VERB_GET_EAPD_BTLENABLE      0x0f0c
#define HDA_VERB_SET_EAPD_BTLENABLE      0x070c
#define HDA_VERB_GET_AMP_GAIN_MUTE       0x0b00
#define HDA_VERB_SET_AMP_GAIN_MUTE       0x0300

#define HDA_PAR_VENDOR_ID                0x00
#define HDA_PAR_SUBSYSTEM_ID             0x01
#define HDA_PAR_REV_ID                   0x02
#define HDA_PAR_NODE_COUNT               0x04
#define HDA_PAR_FUNCTION_TYPE            0x05
#define HDA_PAR_AUDIO_WIDGET_CAP         0x09
#define HDA_PAR_PCM                      0x0a
#define HDA_PAR_STREAM                   0x0b
#define HDA_PAR_PIN_CAP                  0x0c
#define HDA_PAR_AMP_IN_CAP               0x0d
#define HDA_PAR_CONNLIST_LEN             0x0e
#define HDA_PAR_POWER_STATE              0x0f
#define HDA_PAR_AMP_OUT_CAP              0x12

/* Power state definitions */
#define HDA_STATE_D0              0
#define HDA_STATE_D3              3

#define HDA_AMP_MUTE             (1<<7)
#define HDA_AMP_GAIN             0x7f
#define HDA_AMP_SET_RIGHT        (1<<12)
#define HDA_AMP_SET_LEFT         (1<<13)
#define HDA_AMP_GET_LEFT         (1<<13)

#endif /* _SOUND_HDA_H_ */
