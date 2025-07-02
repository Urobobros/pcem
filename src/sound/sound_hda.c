#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "pci.h"
#include "sound.h"
#include "sound_hda.h"

#define USE_OPENAL
#ifdef USE_OPENAL
#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#endif

/*
 * This is a greatly simplified Intel HDA device.  It exposes a PCI
 * multimedia audio controller and provides a single playback stream so
 * that guest drivers can initialise.  The implementation is based on
 * the layout used by QEMU but does not emulate the full specification
 * yet.  It now supports a minimal buffer descriptor list (BDL) engine so
 * the guest can feed real PCM samples.
 */

typedef struct hda_state_t {
        uint8_t pci_command;
        uint32_t bar0;
        uint8_t int_line;
        int card;
        mem_mapping_t mmio;

        uint64_t bdl_addr;
        uint16_t lvi;
        uint16_t cvi;
        uint32_t desc_pos;
        uint64_t desc_addr;
        uint8_t run;
        uint8_t irq_enable;
        uint8_t irq_pending;

        uint64_t bdl_addr2;
        uint16_t lvi2;
        uint16_t cvi2;
        uint32_t desc_pos2;
        uint64_t desc_addr2;
        uint8_t run2;
        uint8_t irq_enable2;
        uint8_t irq_pending2;

        uint64_t cap_bdl_addr;
        uint16_t cap_lvi;
        uint16_t cap_cvi;
        uint32_t cap_desc_pos;
        uint64_t cap_desc_addr;
        uint8_t cap_run;
        uint8_t cap_irq_enable;
        uint8_t cap_irq_pending;

#ifdef USE_OPENAL
        ALCdevice *cap_dev;
        int16_t   cap_buffer[4096];
        int       cap_buf_len;
        int       cap_buf_pos;
#endif

        /* Basic global registers */
        uint32_t gctl;
        uint32_t statests;
        uint32_t wakeen;
        uint8_t  pwr_state;

        /* Minimal CORB/RIRB state for codec verbs */
        uint32_t corb_lbase;
        uint32_t corb_ubase;
        uint8_t  corb_rp;
        uint8_t  corb_wp;
        uint8_t  corb_ctl;
        uint8_t  corb_sts;

        uint32_t rirb_lbase;
        uint32_t rirb_ubase;
        uint8_t  rirb_wp;
        uint8_t  rirb_ctl;
        uint8_t  rirb_sts;

        /* Simple beep generator */
        uint8_t  beep;
        double   beep_phase;
} hda_state_t;

/* Offsets for our very small register set */
#define HDA_REG_GCAP   0x00
#define HDA_REG_GCTL   0x08
#define HDA_REG_WAKEEN 0x0c
#define HDA_REG_STATESTS 0x0e

/* Minimal CORB / RIRB registers */
#define HDA_REG_CORBLBASE 0x40
#define HDA_REG_CORBUBASE 0x44
#define HDA_REG_CORBWP    0x48
#define HDA_REG_CORBRP    0x4a
#define HDA_REG_CORBCTL   0x4c
#define HDA_REG_CORBSTS   0x4d
#define HDA_REG_RIRBLBASE 0x50
#define HDA_REG_RIRBUBASE 0x54
#define HDA_REG_RIRBWP    0x58
#define HDA_REG_RIRBCTL   0x5c
#define HDA_REG_RIRBSTS   0x5d

static void hda_process_corb(hda_state_t *hda);
static void hda_raise_irq(hda_state_t *hda);
static void hda_raise_irq2(hda_state_t *hda);
static void hda_raise_cap_irq(hda_state_t *hda);
static void hda_update_irq(hda_state_t *hda);

#define HDA_REG_BEEP   0x70
#define HDA_REG_BDLPL  0x100
#define HDA_REG_BDLPU  0x104
#define HDA_REG_LVI    0x108
#define HDA_REG_CTL    0x110
#define HDA_REG_STS    0x114

#define HDA_REG_CBDLPL 0x120
#define HDA_REG_CBDLPU 0x124
#define HDA_REG_CLVI   0x128
#define HDA_REG_CCTL   0x130
#define HDA_REG_CSTS   0x134
#define HDA_REG_POSL   0x138
#define HDA_REG_POSU   0x13c
#define HDA_REG_CPOSL  0x140
#define HDA_REG_CPOSU  0x144
#define HDA_REG_BDLPL2 0x150
#define HDA_REG_BDLPU2 0x154
#define HDA_REG_LVI2   0x158
#define HDA_REG_CTL2   0x160
#define HDA_REG_STS2   0x164
#define HDA_REG_POSL2  0x168
#define HDA_REG_POSU2  0x16c

static uint8_t hda_mmio_readb(uint32_t addr, void *p) { return 0xff; }
static uint16_t hda_mmio_readw(uint32_t addr, void *p) { return 0xffff; }
static uint32_t hda_mmio_readl(uint32_t addr, void *p) {
        hda_state_t *hda = (hda_state_t *)p;

       switch (addr) {
       case HDA_REG_GCAP:
               return 0x0004; /* minimal GCAP: two output streams and one input */
       case HDA_REG_GCTL:
               return hda->gctl;
       case HDA_REG_WAKEEN:
               return hda->wakeen;
       case HDA_REG_STATESTS:
               return hda->statests;
       case HDA_REG_CORBLBASE:
               return hda->corb_lbase;
       case HDA_REG_CORBUBASE:
               return hda->corb_ubase;
       case HDA_REG_CORBWP:
               return hda->corb_wp;
       case HDA_REG_CORBRP:
               return hda->corb_rp;
       case HDA_REG_CORBCTL:
               return hda->corb_ctl;
       case HDA_REG_CORBSTS:
               return hda->corb_sts;
       case HDA_REG_RIRBLBASE:
               return hda->rirb_lbase;
       case HDA_REG_RIRBUBASE:
               return hda->rirb_ubase;
       case HDA_REG_RIRBWP:
               return hda->rirb_wp;
       case HDA_REG_RIRBCTL:
               return hda->rirb_ctl;
       case HDA_REG_RIRBSTS:
               return hda->rirb_sts;
       case HDA_REG_BEEP:
               return hda->beep;
       case HDA_REG_BDLPL:
                return (uint32_t)hda->bdl_addr;
        case HDA_REG_BDLPU:
                return (uint32_t)(hda->bdl_addr >> 32);
        case HDA_REG_LVI:
                return hda->lvi;
        case HDA_REG_CTL:
                return (hda->run ? 1 : 0) | (hda->irq_enable ? 2 : 0);
        case HDA_REG_STS:
                return (hda->irq_pending ? 1 : 0) |
                       (hda->cap_irq_pending ? 2 : 0);
        case HDA_REG_CBDLPL:
                return (uint32_t)hda->cap_bdl_addr;
        case HDA_REG_CBDLPU:
                return (uint32_t)(hda->cap_bdl_addr >> 32);
        case HDA_REG_CLVI:
                return hda->cap_lvi;
        case HDA_REG_CCTL:
                return (hda->cap_run ? 1 : 0) | (hda->cap_irq_enable ? 2 : 0);
        case HDA_REG_CSTS:
                return hda->cap_irq_pending ? 1 : 0;
        case HDA_REG_POSL:
                return (uint32_t)hda->desc_addr;
        case HDA_REG_POSU:
                return (uint32_t)(hda->desc_addr >> 32);
        case HDA_REG_CPOSL:
                return (uint32_t)hda->cap_desc_addr;
        case HDA_REG_CPOSU:
                return (uint32_t)(hda->cap_desc_addr >> 32);
        case HDA_REG_BDLPL2:
                return (uint32_t)hda->bdl_addr2;
        case HDA_REG_BDLPU2:
                return (uint32_t)(hda->bdl_addr2 >> 32);
        case HDA_REG_LVI2:
                return hda->lvi2;
        case HDA_REG_CTL2:
                return (hda->run2 ? 1 : 0) | (hda->irq_enable2 ? 2 : 0);
        case HDA_REG_STS2:
                return hda->irq_pending2 ? 1 : 0;
        case HDA_REG_POSL2:
                return (uint32_t)hda->desc_addr2;
        case HDA_REG_POSU2:
                return (uint32_t)(hda->desc_addr2 >> 32);
        default:
                return 0xffffffff;
        }
}

static void hda_mmio_writeb(uint32_t addr, uint8_t val, void *p) {}
static void hda_mmio_writew(uint32_t addr, uint16_t val, void *p) {}
static void hda_mmio_writel(uint32_t addr, uint32_t val, void *p) {
        hda_state_t *hda = (hda_state_t *)p;

       switch (addr) {
       case HDA_REG_GCTL:
               {
                       uint32_t old = hda->gctl;
                       hda->gctl = val;
                       if (!(old & 1) && (val & 1)) {
                               hda->pwr_state = HDA_STATE_D0;
                               hda->statests |= 1;
                               if (hda->wakeen & 1)
                                       hda_raise_irq(hda);
                       } else if ((old & 1) && !(val & 1)) {
                               hda->pwr_state = HDA_STATE_D3;
                       }
               }
               break;
       case HDA_REG_STATESTS:
               hda->statests &= ~val;
               break;
       case HDA_REG_WAKEEN:
               hda->wakeen = val;
               break;
       case HDA_REG_BDLPL:
                hda->bdl_addr = (hda->bdl_addr & 0xffffffff00000000ULL) | val;
                break;
        case HDA_REG_BDLPU:
                hda->bdl_addr = (hda->bdl_addr & 0xffffffffULL) | ((uint64_t)val << 32);
                break;
        case HDA_REG_LVI:
                hda->lvi = val & 0xff;
                hda->cvi = 0;
                hda->desc_pos = 0;
                break;
        case HDA_REG_CTL:
                hda->run = val & 1;
                hda->irq_enable = val & 2 ? 1 : 0;
                if (hda->run) {
                        hda->cvi = 0;
                        hda->desc_pos = 0;
                }
                if (!hda->irq_enable)
                        pci_clear_irq(hda->card, PCI_INTA);
                else if (hda->irq_pending)
                        pci_set_irq(hda->card, PCI_INTA);
                break;
       case HDA_REG_STS:
                if (val & 1)
                        hda->irq_pending = 0;
                if (val & 2)
                        hda->cap_irq_pending = 0;
                hda_update_irq(hda);
                break;
        case HDA_REG_CBDLPL:
                hda->cap_bdl_addr = (hda->cap_bdl_addr & 0xffffffff00000000ULL) | val;
                break;
       case HDA_REG_CBDLPU:
               hda->cap_bdl_addr = (hda->cap_bdl_addr & 0xffffffffULL) | ((uint64_t)val << 32);
               break;
        case HDA_REG_CLVI:
                hda->cap_lvi = val & 0xff;
                hda->cap_cvi = 0;
                hda->cap_desc_pos = 0;
                break;
        case HDA_REG_CCTL:
                hda->cap_run = val & 1;
                hda->cap_irq_enable = val & 2 ? 1 : 0;
                if (hda->cap_run) {
                        hda->cap_cvi = 0;
                        hda->cap_desc_pos = 0;
                }
                if (!hda->cap_irq_enable)
                        pci_clear_irq(hda->card, PCI_INTA);
                else if (hda->cap_irq_pending)
                        pci_set_irq(hda->card, PCI_INTA);
                break;
       case HDA_REG_CSTS:
               if (val & 1) {
                       hda->cap_irq_pending = 0;
                       pci_clear_irq(hda->card, PCI_INTA);
               }
               break;
       case HDA_REG_CORBLBASE:
               hda->corb_lbase = val;
               break;
       case HDA_REG_CORBUBASE:
               hda->corb_ubase = val;
               break;
       case HDA_REG_CORBWP:
               hda->corb_wp = val & 0xff;
               hda_process_corb(hda);
               break;
       case HDA_REG_CORBRP:
               hda->corb_rp = val & 0xff;
               break;
       case HDA_REG_CORBCTL:
               hda->corb_ctl = val & 0x03;
               hda_process_corb(hda);
               break;
       case HDA_REG_CORBSTS:
               hda->corb_sts &= ~val;
               break;
       case HDA_REG_RIRBLBASE:
               hda->rirb_lbase = val;
               break;
       case HDA_REG_RIRBUBASE:
               hda->rirb_ubase = val;
               break;
       case HDA_REG_RIRBWP:
               hda->rirb_wp = val & 0xff;
               break;
       case HDA_REG_RIRBCTL:
               hda->rirb_ctl = val & 0x03;
               break;
       case HDA_REG_RIRBSTS:
               hda->rirb_sts &= ~val;
               break;
       case HDA_REG_BEEP:
               hda->beep = val & 0xff;
               hda->beep_phase = 0.0;
               break;
       case HDA_REG_BDLPL2:
               hda->bdl_addr2 = (hda->bdl_addr2 & 0xffffffff00000000ULL) | val;
               break;
       case HDA_REG_BDLPU2:
               hda->bdl_addr2 = (hda->bdl_addr2 & 0xffffffffULL) | ((uint64_t)val << 32);
               break;
       case HDA_REG_LVI2:
               hda->lvi2 = val & 0xff;
               hda->cvi2 = 0;
               hda->desc_pos2 = 0;
               break;
       case HDA_REG_CTL2:
               hda->run2 = val & 1;
               hda->irq_enable2 = val & 2 ? 1 : 0;
               if (hda->run2) {
                       hda->cvi2 = 0;
                       hda->desc_pos2 = 0;
               }
               hda_update_irq(hda);
               break;
       case HDA_REG_STS2:
               if (val & 1)
                       hda->irq_pending2 = 0;
               hda_update_irq(hda);
               break;
       }
}

static void hda_update_mapping(hda_state_t *hda) {
        if (!(hda->pci_command & PCI_COMMAND_MEM)) {
                mem_mapping_disable(&hda->mmio);
                return;
        }

        mem_mapping_set_addr(&hda->mmio, hda->bar0 & ~0xf, 0x4000);
}

static uint8_t hda_pci_read(int func, int addr, void *p) {
        hda_state_t *hda = (hda_state_t *)p;

        if (func)
                return 0;

        switch (addr) {
        case 0x00:
                return 0x86; /* vendor low */
        case 0x01:
                return 0x80; /* vendor high (0x8086) */
        case 0x02:
                return 0x68; /* device low (0x2668 - ICH6) */
        case 0x03:
                return 0x26; /* device high */
        case 0x04:
                return hda->pci_command;
        case 0x0a:
                return 0x03; /* subclass - HDA */
        case 0x0b:
                return 0x04; /* multimedia */
        case 0x10:
                return (hda->bar0 & 0xff) | 1;
        case 0x11:
                return hda->bar0 >> 8;
        case 0x12:
                return hda->bar0 >> 16;
        case 0x13:
                return hda->bar0 >> 24;
        case 0x3c:
                return hda->int_line;
        }

        return 0;
}

static void hda_pci_write(int func, int addr, uint8_t val, void *p) {
        hda_state_t *hda = (hda_state_t *)p;

        if (func)
                return;

        switch (addr) {
        case 0x04:
                hda->pci_command = val;
                hda_update_mapping(hda);
                break;
        case 0x10:
                hda->bar0 = (hda->bar0 & 0xffffff00) | (val & 0xf0);
                hda_update_mapping(hda);
                break;
        case 0x11:
                hda->bar0 = (hda->bar0 & 0xffff00ff) | (val << 8);
                hda_update_mapping(hda);
                break;
        case 0x12:
                hda->bar0 = (hda->bar0 & 0xff00ffff) | (val << 16);
                hda_update_mapping(hda);
                break;
        case 0x13:
                hda->bar0 = (hda->bar0 & 0x00ffffff) | (val << 24);
                hda_update_mapping(hda);
                break;
        }
}

/*
 * The real Intel HDA hardware streams PCM data from guest memory.  This
 * emulation implements a very small subset of that functionality using a
 * buffer descriptor list (BDL).  Only a single playback stream is
 * supported and interrupts are generated when each descriptor completes so
 * the guest can queue more buffers.
 */
static void hda_load_descriptor(hda_state_t *hda) {
        if (hda->cvi > hda->lvi)
                return;

        uint32_t off = hda->cvi * 16;
        uint64_t daddr = mem_readl_phys(hda->bdl_addr + off) | ((uint64_t)mem_readl_phys(hda->bdl_addr + off + 4) << 32);
        uint32_t dlen = mem_readl_phys(hda->bdl_addr + off + 8);

        hda->desc_addr = daddr;
        hda->desc_pos = dlen;
        hda->cvi++;
        if (hda->cvi > hda->lvi)
                hda->cvi = 0;
}

static void hda_load_descriptor2(hda_state_t *hda) {
        if (hda->cvi2 > hda->lvi2)
                return;

        uint32_t off = hda->cvi2 * 16;
        uint64_t daddr = mem_readl_phys(hda->bdl_addr2 + off) |
                         ((uint64_t)mem_readl_phys(hda->bdl_addr2 + off + 4) << 32);
        uint32_t dlen = mem_readl_phys(hda->bdl_addr2 + off + 8);

        hda->desc_addr2 = daddr;
        hda->desc_pos2 = dlen;
        hda->cvi2++;
        if (hda->cvi2 > hda->lvi2)
                hda->cvi2 = 0;
}

static void hda_load_capture_descriptor(hda_state_t *hda) {
        if (hda->cap_cvi > hda->cap_lvi)
                return;

        uint32_t off = hda->cap_cvi * 16;
        uint64_t daddr = mem_readl_phys(hda->cap_bdl_addr + off) |
                         ((uint64_t)mem_readl_phys(hda->cap_bdl_addr + off + 4) << 32);
        uint32_t dlen = mem_readl_phys(hda->cap_bdl_addr + off + 8);

        hda->cap_desc_addr = daddr;
        hda->cap_desc_pos = dlen;
        hda->cap_cvi++;
        if (hda->cap_cvi > hda->cap_lvi)
                hda->cap_cvi = 0;
}

static void hda_raise_irq(hda_state_t *hda) {
        hda->irq_pending = 1;
        hda_update_irq(hda);
}

static void hda_raise_irq2(hda_state_t *hda) {
        hda->irq_pending2 = 1;
        hda_update_irq(hda);
}

static void hda_raise_cap_irq(hda_state_t *hda) {
        hda->cap_irq_pending = 1;
        hda_update_irq(hda);
}

static void hda_update_irq(hda_state_t *hda) {
        if ((hda->irq_enable && hda->irq_pending) ||
            (hda->irq_enable2 && hda->irq_pending2) ||
            (hda->cap_irq_enable && hda->cap_irq_pending))
                pci_set_irq(hda->card, PCI_INTA);
        else
                pci_clear_irq(hda->card, PCI_INTA);
}

#ifdef USE_OPENAL
static void hda_fill_capture(hda_state_t *hda) {
        if (!hda->cap_dev)
                return;

        if (hda->cap_buf_len >= 2048)
                return;

        ALint avail = 0;
        alcGetIntegerv(hda->cap_dev, ALC_CAPTURE_SAMPLES, sizeof(avail), &avail);
        if (avail <= 0)
                return;

        if (avail > 2048 - hda->cap_buf_len)
                avail = 2048 - hda->cap_buf_len;

        alcCaptureSamples(hda->cap_dev,
                          hda->cap_buffer + hda->cap_buf_len * 2,
                          avail);
        hda->cap_buf_len += avail;
}
#endif

static uint32_t hda_handle_verb(uint32_t verb)
{
        uint16_t cmd = (verb >> 8) & 0xfff;
        uint8_t parm = verb & 0xff;

        if (cmd == HDA_VERB_GET_PARAMETER) {
                if (parm == HDA_PAR_VENDOR_ID)
                        return 0x80862880; /* Intel */
        }

        return 0;
}

static void hda_process_corb(hda_state_t *hda)
{
        if (!(hda->corb_ctl & 2))
                return;

        while (hda->corb_rp != hda->corb_wp) {
                hda->corb_rp = (hda->corb_rp + 1) & 0xff;
                uint64_t addr = ((uint64_t)hda->corb_ubase << 32) | hda->corb_lbase;
                uint32_t verb = mem_readl_phys(addr + 4 * hda->corb_rp);
                uint32_t resp = hda_handle_verb(verb);

                addr = ((uint64_t)hda->rirb_ubase << 32) | hda->rirb_lbase;
                hda->rirb_wp = (hda->rirb_wp + 1) & 0xff;
                mem_writel_phys(addr + 8 * hda->rirb_wp, resp);
                mem_writel_phys(addr + 8 * hda->rirb_wp + 4, 0);
                hda->rirb_sts |= 1;
                if (hda->rirb_ctl & 1)
                        hda_raise_irq(hda);
        }
}

static void hda_get_buffer(int32_t *buffer, int len, void *p) {
        hda_state_t *hda = (hda_state_t *)p;

        for (int i = 0; i < len * 2; i += 2) {
                if (hda->run) {
                        if (!hda->desc_pos) {
                                hda_raise_irq(hda);
                                hda_load_descriptor(hda);
                        }

                        if (hda->desc_pos >= 4) {
                                int16_t l = mem_readw_phys(hda->desc_addr);
                                int16_t r = mem_readw_phys(hda->desc_addr + 2);
                                hda->desc_addr += 4;
                                hda->desc_pos -= 4;
                                if (!hda->desc_pos)
                                        hda_raise_irq(hda);

                                buffer[i] += l;
                                buffer[i + 1] += r;
                                continue;
                        }
                }

                if (hda->run2) {
                        if (!hda->desc_pos2) {
                                hda_raise_irq2(hda);
                                hda_load_descriptor2(hda);
                        }

                        if (hda->desc_pos2 >= 4) {
                                int16_t l2 = mem_readw_phys(hda->desc_addr2);
                                int16_t r2 = mem_readw_phys(hda->desc_addr2 + 2);
                                hda->desc_addr2 += 4;
                                hda->desc_pos2 -= 4;
                                if (!hda->desc_pos2)
                                        hda_raise_irq2(hda);

                                buffer[i] += l2;
                                buffer[i + 1] += r2;
                                continue;
                        }
                }

                if (hda->cap_run) {
                        if (!hda->cap_desc_pos) {
                                hda_raise_cap_irq(hda);
                                hda_load_capture_descriptor(hda);
                        }

#ifdef USE_OPENAL
                        hda_fill_capture(hda);
#endif

                        if (hda->cap_desc_pos >= 4) {
#ifdef USE_OPENAL
                                int16_t l = 0, r = 0;
                                if (hda->cap_buf_len > 0) {
                                        l = hda->cap_buffer[hda->cap_buf_pos * 2];
                                        r = hda->cap_buffer[hda->cap_buf_pos * 2 + 1];
                                        hda->cap_buf_pos++;
                                        hda->cap_buf_len--;
                                        if (hda->cap_buf_pos >= 2048)
                                                hda->cap_buf_pos = 0;
                                }
                                mem_writew_phys(hda->cap_desc_addr, l);
                                mem_writew_phys(hda->cap_desc_addr + 2, r);
#else
                                mem_writew_phys(hda->cap_desc_addr, 0);
                                mem_writew_phys(hda->cap_desc_addr + 2, 0);
#endif
                                hda->cap_desc_addr += 4;
                                hda->cap_desc_pos -= 4;
                                if (!hda->cap_desc_pos)
                                        hda_raise_cap_irq(hda);
                        }
                }

                /* If not running output silence */
                buffer[i] += 0;
                buffer[i + 1] += 0;

                if (hda->beep) {
                        double freq = (double)hda->beep * 40.0;
                        hda->beep_phase += freq / 48000.0;
                        if (hda->beep_phase >= 1.0)
                                hda->beep_phase -= 1.0;
                        int16_t s = (hda->beep_phase < 0.5) ? 3000 : -3000;
                        buffer[i] += s;
                        buffer[i + 1] += s;
                }
        }
}

static void *hda_init() {
        hda_state_t *hda = malloc(sizeof(hda_state_t));
        if (!hda)
                return NULL;

       memset(hda, 0, sizeof(hda_state_t));
       hda->bar0 = 0xfebf0000;
       hda->int_line = 5;
       hda->gctl = 0;
       hda->statests = 0;
       hda->wakeen = 0;
       hda->pwr_state = HDA_STATE_D0;

       hda->corb_lbase = 0;
       hda->corb_ubase = 0;
       hda->corb_rp = 0;
       hda->corb_wp = 0;
       hda->corb_ctl = 0;
       hda->corb_sts = 0;

       hda->rirb_lbase = 0;
       hda->rirb_ubase = 0;
       hda->rirb_wp = 0xff;
       hda->rirb_ctl = 0;
       hda->rirb_sts = 0;

       hda->beep = 0;
       hda->beep_phase = 0.0;

        hda->cap_bdl_addr = 0;
        hda->cap_lvi = 0;
        hda->cap_cvi = 0;
        hda->cap_desc_pos = 0;

        hda->bdl_addr2 = 0;
        hda->lvi2 = 0;
        hda->cvi2 = 0;
        hda->desc_pos2 = 0;
        hda->desc_addr2 = 0;
        hda->run2 = 0;
        hda->irq_enable2 = 0;
        hda->irq_pending2 = 0;
#ifdef USE_OPENAL
        hda->cap_dev = alcCaptureOpenDevice(NULL, 48000, AL_FORMAT_STEREO16, 4800);
        if (hda->cap_dev) {
                alcCaptureStart(hda->cap_dev);
        }
        hda->cap_buf_len = 0;
        hda->cap_buf_pos = 0;
#endif

        sound_add_handler(hda_get_buffer, hda);

        mem_mapping_add(&hda->mmio, 0, 0, hda_mmio_readb, hda_mmio_readw, hda_mmio_readl, hda_mmio_writeb, hda_mmio_writew,
                        hda_mmio_writel, NULL, MEM_MAPPING_EXTERNAL, hda);
        mem_mapping_disable(&hda->mmio);

        hda->card = pci_add(hda_pci_read, hda_pci_write, hda);
        hda_update_mapping(hda);

        return hda;
}

static void hda_close(void *p) {
        hda_state_t *hda = (hda_state_t *)p;

#ifdef USE_OPENAL
        if (hda->cap_dev) {
                alcCaptureStop(hda->cap_dev);
                alcCaptureCloseDevice(hda->cap_dev);
        }
#endif

        mem_mapping_remove(&hda->mmio);
        /* PCI bus does not currently support hot removal */
        free(hda);
}

static void hda_speed_changed(void *p) { (void)p; }

device_t hda_device = {"Intel High Definition Audio", DEVICE_PCI, hda_init, hda_close, NULL, hda_speed_changed, NULL, NULL, NULL};
