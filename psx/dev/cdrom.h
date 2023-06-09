#ifndef CDROM_H
#define CDROM_H

#include <stdint.h>
#include <stdio.h>

#include "ic.h"

#define PSX_CDROM_BEGIN 0x1f801800
#define PSX_CDROM_SIZE  0x4
#define PSX_CDROM_END   0x1f801803

#define CD_SECTOR_SIZE 0x930
#define CD_SECTORS_PER_SECOND 75

#define STAT_INDEX_MASK   0x3
#define STAT_ADPBUSY_MASK 0x4
#define STAT_PRMEMPT_MASK 0x8
#define STAT_PRMWRDY_MASK 0x10
#define STAT_RSLRRDY_MASK 0x20
#define STAT_DRQSTS_MASK  0x40
#define STAT_BUSYSTS_MASK 0x80
#define STAT_INDEX   (cdrom->status & STAT_INDEX_MASK)
#define STAT_ADPBUSY (cdrom->status & STAT_ADPBUSY_MASK)
#define STAT_PRMEMPT (cdrom->status & STAT_PRMEMPT_MASK)
#define STAT_PRMWRDY (cdrom->status & STAT_PRMWRDY_MASK)
#define STAT_RSLRRDY (cdrom->status & STAT_RSLRRDY_MASK)
#define STAT_DRQSTS  (cdrom->status & STAT_DRQSTS_MASK)
#define STAT_BUSYSTS (cdrom->status & STAT_BUSYSTS_MASK)
#define SET_BITS(reg, mask, v) { cdrom->reg &= ~mask; cdrom->reg |= v & mask; }
#define IFR_INT  0x07
#define IFR_INT1 0x01
#define IFR_INT2 0x02
#define IFR_INT3 0x03
#define IFR_INT4 0x04
#define IFR_INT5 0x05
#define IFR_INT6 0x06
#define IFR_INT7 0x07

#define GETSTAT_ERROR      0x01
#define GETSTAT_MOTOR      0x02
#define GETSTAT_SEEKERROR  0x04
#define GETSTAT_IDERROR    0x08
#define GETSTAT_TRAYOPEN   0x10
#define GETSTAT_READ       0x20
#define GETSTAT_SEEK       0x40
#define GETSTAT_PLAY       0x80

/*
  7   Speed       (0=Normal speed, 1=Double speed)
  6   XA-ADPCM    (0=Off, 1=Send XA-ADPCM sectors to SPU Audio Input)
  5   Sector Size (0=800h=DataOnly, 1=924h=WholeSectorExceptSyncBytes)
  4   Ignore Bit  (0=Normal, 1=Ignore Sector Size and Setloc position)
  3   XA-Filter   (0=Off, 1=Process only XA-ADPCM sectors that match Setfilter)
  2   Report      (0=Off, 1=Enable Report-Interrupts for Audio Play)
  1   AutoPause   (0=Off, 1=Auto Pause upon End of Track) ;for Audio Play
  0   CDDA        (0=Off, 1=Allow to Read CD-DA Sectors; ignore missing EDC)
*/

#define MODE_CDDA           0x01
#define MODE_AUTOPAUSE      0x02
#define MODE_REPORT         0x04
#define MODE_XA_FILTER      0x08
#define MODE_IGNORE         0x10
#define MODE_SECTOR_SIZE    0x20
#define MODE_XA_ADPCM       0x40
#define MODE_SPEED          0x80

/*
  0-4 0    Not used (should be zero)
  5   SMEN Want Command Start Interrupt on Next Command (0=No change, 1=Yes)
  6   BFWR ...
  7   BFRD Want Data         (0=No/Reset Data Fifo, 1=Yes/Load Data Fifo)
*/

#define REQ_SMEN 0x20
#define REQ_BFWR 0x40
#define REQ_BFRD 0x80

typedef struct {
    uint32_t io_base, io_size;

    psx_ic_t* ic;

    uint8_t status;
    uint8_t ifr;
    uint8_t ier;
    uint8_t request;

    uint8_t pfifo[16];
    uint8_t rfifo[16];
    int pfifo_index;
    int rfifo_index;

    uint8_t read_buf[CD_SECTOR_SIZE];
    uint8_t dfifo[CD_SECTOR_SIZE];
    int dfifo_index;
    int dfifo_full;

    // GetStat bits
    uint8_t stat;

    // API
    int tray_open;

    // Setloc
    uint8_t seek_ss, seek_mm, seek_sect;
    uint32_t seek_offset;

    // Setmode
    uint8_t mode;

    int irq_delay;
    uint8_t command;
    uint8_t delayed_response_command;
    int delayed_response;

    const char* path;
    FILE* disc;
} psx_cdrom_t;

psx_cdrom_t* psx_cdrom_create();
void psx_cdrom_init(psx_cdrom_t*, psx_ic_t*);
uint32_t psx_cdrom_read32(psx_cdrom_t*, uint32_t);
uint16_t psx_cdrom_read16(psx_cdrom_t*, uint32_t);
uint8_t psx_cdrom_read8(psx_cdrom_t*, uint32_t);
void psx_cdrom_write32(psx_cdrom_t*, uint32_t, uint32_t);
void psx_cdrom_write16(psx_cdrom_t*, uint32_t, uint16_t);
void psx_cdrom_write8(psx_cdrom_t*, uint32_t, uint8_t);
void psx_cdrom_update(psx_cdrom_t*);
void psx_cdrom_destroy(psx_cdrom_t*);
void psx_cdrom_open(psx_cdrom_t*, const char*);
void psx_cdrom_close(psx_cdrom_t*);

// Commands
void cdrom_cmd_unimplemented(psx_cdrom_t*);
void cdrom_cmd_getstat(psx_cdrom_t*);
void cdrom_cmd_setloc(psx_cdrom_t*);
void cdrom_cmd_readn(psx_cdrom_t*);
void cdrom_cmd_stop(psx_cdrom_t*);
void cdrom_cmd_pause(psx_cdrom_t*);
void cdrom_cmd_init(psx_cdrom_t*);
void cdrom_cmd_unmute(psx_cdrom_t*);
void cdrom_cmd_setfilter(psx_cdrom_t*);
void cdrom_cmd_setmode(psx_cdrom_t*);
void cdrom_cmd_getlocl(psx_cdrom_t*);
void cdrom_cmd_getlocp(psx_cdrom_t*);
void cdrom_cmd_gettn(psx_cdrom_t*);
void cdrom_cmd_gettd(psx_cdrom_t*);
void cdrom_cmd_seekl(psx_cdrom_t*);
void cdrom_cmd_seekp(psx_cdrom_t*);
void cdrom_cmd_test(psx_cdrom_t*);
void cdrom_cmd_getid(psx_cdrom_t*);
void cdrom_cmd_reads(psx_cdrom_t*);
void cdrom_cmd_readtoc(psx_cdrom_t*);

#endif