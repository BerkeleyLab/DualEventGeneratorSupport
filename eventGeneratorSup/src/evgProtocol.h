#ifndef _EVENT_GENERATOR_PROTOCOL_
#define _EVENT_GENERATOR_PROTOCOL_

#include <stdint.h>

#define EVG_PROTOCOL_UDP_EPICS_PORT     58762
#define EVG_PROTOCOL_UDP_STATUS_PORT    58763
#define EVG_PROTOCOL_MAGIC              0xBD008426
#define EVG_PROTOCOL_MAGIC_SWAPPED      0x268400BD
#define EVG_PROTOCOL_ARG_CAPACITY       350
#define EVG_PROTOCOL_EVG_COUNT          2

struct evgPacket {
    uint32_t    magic;
    uint32_t    nonce;
    uint32_t    command;
    uint32_t    args[EVG_PROTOCOL_ARG_CAPACITY];
};

struct evgStatusPacket {
    uint32_t    magic;
    uint32_t    pkNumber;
    uint32_t    sequencerStatus[EVG_PROTOCOL_EVG_COUNT];
};

#define EVG_PROTOCOL_SIZE_TO_ARG_COUNT(s) (EVG_PROTOCOL_ARG_CAPACITY - \
                    ((sizeof(struct evgPacket)-(s))/sizeof(uint32_t)))
#define EVG_PROTOCOL_ARG_COUNT_TO_SIZE(a) (sizeof(struct evgPacket) - \
                        ((EVG_PROTOCOL_ARG_CAPACITY - (a)) * sizeof(uint32_t)))
#define EVG_PROTOCOL_ARG_COUNT_TO_U32_COUNT(a) \
                    ((sizeof(struct evgPacket) / sizeof(uint32_t)) - \
                                            (EVG_PROTOCOL_ARG_CAPACITY - (a)))
#define EVG_PROTOCOL_U32_COUNT_TO_ARG_COUNT(u) (EVG_PROTOCOL_ARG_CAPACITY - \
                    (((sizeof(struct evgPacket)/sizeof(uint32_t)))-(u)))

#define EVG_PROTOCOL_CMD_MASK_HI             0xF000
#define EVG_PROTOCOL_CMD_MASK_LO             0x0F00
#define EVG_PROTOCOL_CMD_MASK_IDX            0x00FF

#define EVG_PROTOCOL_CMD_HI_LONGIN           0x0000
#define EVG_PROTOCOL_CMD_LONGIN_LO_GENERIC      0x000
# define EVG_PROTOCOL_CMD_LONGIN_IDX_FIRMWARE_BUILD_DATE 0x00
# define EVG_PROTOCOL_CMD_LONGIN_IDX_SOFTWARE_BUILD_DATE 0x01
#define EVG_PROTOCOL_CMD_LONGIN_LO_SEQ_STATUS   0x100

#define EVG_PROTOCOL_CMD_HI_LONGOUT          0x1000
# define EVG_PROTOCOL_CMD_LONGOUT_LO_NO_VALUE   0x000
#  define EVG_PROTOCOL_CMD_LONGOUT_NV_IDX_CLEAR_POWERUP_STATUS  0x00
#  define EVG_PROTOCOL_CMD_LONGOUT_NV_IDX_RESYNC_TOD            0x02
#  define EVG_PROTOCOL_CMD_LONGOUT_NV_IDX_TRIGGER_INJECTION     0x03
# define EVG_PROTOCOL_CMD_LONGOUT_LO_SW_TRIGGER 0x100
# define EVG_PROTOCOL_CMD_LONGOUT_LO_HW_TRIGGER 0x200
# define EVG_PROTOCOL_CMD_LONGOUT_LO_SEQ_ENABLE 0x300
#  define EVG_PROTOCOL_CMD_LONGOUT_LO_DIAG_OUT  0x400
# define EVG_PROTOCOL_CMD_LONGOUT_LO_GENERIC    0xF00
#  define EVG_PROTOCOL_CMD_LONGOUT_GENERIC_IDX_REBOOT           0x00
#  define EVG_PROTOCOL_CMD_LONGOUT_GENERIC_IDX_INJECTION_ENABLE 0x01
#  define EVG_PROTOCOL_CMD_LONGOUT_GENERIC_IDX_SWAPOUT_ENABLE   0x02
#  define EVG_PROTOCOL_CMD_LONGOUT_GENERIC_IDX_REALIGN_MGT      0x03

#define EVG_PROTOCOL_CMD_HI_SYSMON           0x2000
# define EVG_PROTOCOL_CMD_SYSMON_LO_INT32       0x000
# define EVG_PROTOCOL_CMD_SYSMON_LO_UINT16_LO   0x100
# define EVG_PROTOCOL_CMD_SYSMON_LO_UINT16_HI   0x200
# define EVG_PROTOCOL_CMD_SYSMON_LO_INT16_LO    0x300
# define EVG_PROTOCOL_CMD_SYSMON_LO_INT16_HI    0x400

#define EVG_PROTOCOL_CMD_HI_WAVEFORM         0x3000
#define EVG_PROTOCOL_CMD_WAVEFORM_LO_SEQUENCE   0x000

/*
 * Special sequence waveform values
 * Make sure the end-of-table value matches the firmware
 */
#define EVG_PROTOCOL_WAVEFORM_SINGLE_WORD_DELAY_LIMIT ((1UL<<24)-1)
#define EVG_PROTOCOL_WAVEFORM_END_OF_TABLE_EVENT_CODE   0x7F

#endif /* _EVENT_GENERATOR_PROTOCOL_ */
