/*
 *  CDEmuD: MMC-3 device mode pages definitions
 *  Copyright (C) 2006-2012 Rok Mandeljc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __CDEMUD_MODE_PAGES_H__
#define __CDEMUD_MODE_PAGES_H__

#pragma pack(1)

enum
{
    MODE_PAGE_CURRENT = 0,
    MODE_PAGE_DEFAULT = 1,
    MODE_PAGE_MASK = 2
};


struct ModePage_GENERAL
{
    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  ps          : 1;
        guint8  __dummy1__  : 1;
        guint8  code        : 6;
    #else
        guint8  code        : 6;
        guint8  __dummy1__  : 1;
        guint8  ps          : 1;
    #endif

    guint8  length;
};


/* Read/Write Error Recovery Parameters Mode Page */
struct ModePage_0x01
{
    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  ps          : 1;
        guint8  __dummy1__  : 1;
        guint8  code        : 6;
    #else
        guint8  code        : 6;
        guint8  __dummy1__  : 1;
        guint8  ps          : 1;
    #endif

    guint8  length;

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  awre        : 1;
        guint8  arre        : 1;
        guint8  tb          : 1;
        guint8  rc          : 1;
        guint8  __dummy2__  : 1;
        guint8  per         : 1;
        guint8  dte         : 1;
        guint8  dcr         : 1;
    #else
        guint8  dcr         : 1;
        guint8  dte         : 1;
        guint8  per         : 1;
        guint8  __dummy2__  : 1;
        guint8  rc          : 1;
        guint8  tb          : 1;
        guint8  arre        : 1;
        guint8  awre        : 1;
    #endif

    guint8  read_retry;

    guint8  __dummy3__[4];

    guint8  write_retry;

    guint8  __dummy4__;

    guint16 recovery;
};

/* CD Device Parameters Mode Page */
struct ModePage_0x0D
{
    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  ps          : 1;
        guint8  __dummy1__  : 1;
        guint8  code        : 6;
    #else
        guint8  code        : 6;
        guint8  __dummy1__  : 1;
        guint8  ps          : 1;
    #endif

    guint8  length;

    guint8  __dummy2__;

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy3__  : 4;
        guint8  inact_mult  : 4;
    #else
        guint8  inact_mult  : 4;
        guint8  __dummy3__  : 4;
    #endif

    guint16 spm;

    guint16 fps;
};

/* CD Audio Control Mode Page */
struct ModePage_0x0E
{
    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  ps          : 1;
        guint8  __dummy1__  : 1;
        guint8  code        : 6;
    #else
        guint8  code        : 6;
        guint8  __dummy1__  : 1;
        guint8  ps          : 1;
    #endif

    guint8  length;

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy2__  : 5;
        guint8  immed       : 1;
        guint8  sotc        : 1;
        guint8  __dummy3__  : 1;
    #else
        guint8  __dummy3__  : 1;
        guint8  sotc        : 1;
        guint8  immed       : 1;
        guint8  __dummy2__  : 5;
    #endif

    guint8  __dummy4__[5];

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy5__  : 4;
        guint8  port0csel   : 4;
    #else
        guint8  port0csel   : 4;
        guint8  __dummy5__  : 4;
    #endif
    guint8  port0vol;

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy6__  : 4;
        guint8  port1csel   : 4;
    #else
        guint8  port1csel   : 4;
        guint8  __dummy6__  : 4;
    #endif
    guint8  port1vol;

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy7__  : 4;
        guint8  port2csel   : 4;
    #else
        guint8  port2csel   : 4;
        guint8  __dummy7__  : 4;
    #endif
    guint8  port2vol;

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy8__  : 4;
        guint8  port3csel   : 4;
    #else
        guint8  port3csel   : 4;
        guint8  __dummy8__  : 4;
    #endif
    guint8  port3vol;
};

/* Power Condition Mode Page */
struct ModePage_0x1A
{
    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  ps          : 1;
        guint8  __dummy1__  : 1;
        guint8  code        : 6;
    #else
        guint8  code        : 6;
        guint8  __dummy1__  : 1;
        guint8  ps          : 1;
    #endif

    guint8  length;

    guint8  __dummy2__;

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy3__  : 6;
        guint8  idle        : 1;
        guint8  stdby       : 1;
    #else
        guint8  stdby       : 1;
        guint8  idle        : 1;
        guint8  __dummy3__  : 6;
    #endif

    guint32 idle_timer;

    guint32 stdby_timer;
};


/* CD/DVD Capabilities and Mechanical Status Mode Page */
struct ModePage_0x2A
{
    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  ps          : 1;
        guint8  __dummy1__  : 1;
        guint8  code        : 6;
    #else
        guint8  code        : 6;
        guint8  __dummy1__  : 1;
        guint8  ps          : 1;
    #endif

    guint8  length;

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy2__  : 2;
        guint8  dvdram_read : 1;
        guint8  dvdr_read   : 1;
        guint8  dvdrom_read : 1;
        guint8  method2     : 1;
        guint8  cdrw_read   : 1;
        guint8  cdr_read    : 1;
    #else
        guint8  cdr_read    : 1;
        guint8  cdrw_read   : 1;
        guint8  method2     : 1;
        guint8  dvdrom_read : 1;
        guint8  dvdr_read   : 1;
        guint8  dvdram_read : 1;
        guint8  __dummy2__  : 2;
    #endif

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy3__  : 2;
        guint8  dvdram_write    : 1;
        guint8  dvdr_write  : 1;
        guint8  __dummy4__  : 1;
        guint8  test_write  : 1;
        guint8  cdrw_write  : 1;
        guint8  cdr_write   : 1;
    #else
        guint8  cdr_write   : 1;
        guint8  cdrw_write  : 1;
        guint8  test_write  : 1;
        guint8  __dummy4__  : 1;
        guint8  dvdr_write  : 1;
        guint8  dvdram_write    : 1;
        guint8  __dummy3__  : 2;
    #endif

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  buf         : 1;
        guint8  multisession    : 1;
        guint8  mode2_form2 : 1;
        guint8  mode2_form1 : 1;
        guint8  dport1      : 1;
        guint8  dport2      : 1;
        guint8  composite   : 1;
        guint8  audio_play  : 1;
    #else
        guint8  audio_play  : 1;
        guint8  composite   : 1;
        guint8  dport2      : 1;
        guint8  dport1      : 1;
        guint8  mode2_form1 : 1;
        guint8  mode2_form2 : 1;
        guint8  multisession    : 1;
        guint8  buf         : 1;
    #endif

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  read_barcode    : 1;
        guint8  upc         : 1;
        guint8  isrc        : 1;
        guint8  c2pointers  : 1;
        guint8  rw_deinterleaved    : 1;
        guint8  rw_supported    : 1;
        guint8  cdda_acc_stream : 1;
        guint8  cdda_cmds   : 1;
    #else
        guint8  cdda_cmds   : 1;
        guint8  cdda_acc_stream : 1;
        guint8  rw_supported    : 1;
        guint8  rw_deinterleaved    : 1;
        guint8  c2pointers  : 1;
        guint8  isrc        : 1;
        guint8  upc         : 1;
        guint8  read_barcode    : 1;
    #endif

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  load_mech   : 3;
        guint8  __dummy5__  : 1;
        guint8  eject       : 1;
        guint8  prvnt_jmp   : 1;
        guint8  lock_state  : 1;
        guint8  lock        : 1;
    #else
        guint8  lock        : 1;
        guint8  lock_state  : 1;
        guint8  prvnt_jmp   : 1;
        guint8  eject       : 1;
        guint8  __dummy5__  : 1;
        guint8  load_mech   : 3;
    #endif

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy6__  : 2;
        guint8  rw_in_leadin    : 1;
        guint8  side_change : 1;
        guint8  sw_slot     : 1;
        guint8  discpresent : 1;
        guint8  sep_mute    : 1;
        guint8  sep_vol_lvls    : 1;
    #else
        guint8  sep_vol_lvls    : 1;
        guint8  sep_mute    : 1;
        guint8  discpresent : 1;
        guint8  sw_slot     : 1;
        guint8  side_change : 1;
        guint8  rw_in_leadin    : 1;
        guint8  __dummy6__  : 2;
    #endif

    guint16 max_read_speed; /* According to older standard */

    guint16 vol_lvls;

    guint16 buf_size;

    guint16 cur_read_speed; /* According to older standard */

    guint8  __dummy9__;

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy10__ : 2;
        guint8  word_length : 2;
        guint8  lsbf        : 1;
        guint8  rck         : 1;
        guint8  bckf        : 1;
        guint8  __dummy11__ : 1;
    #else
        guint8  __dummy11__ : 1;
        guint8  bckf        : 1;
        guint8  rck         : 1;
        guint8  lsbf        : 1;
        guint8  word_length : 2;
        guint8  __dummy10__ : 2;
    #endif

    guint16 max_write_speed; /* According to older standard */

    guint16 cur_write_speed; /* According to older standard */

    guint16 copy_man_rev;

    guint8  __dummy13__[3];

    #if G_BYTE_ORDER == G_BIG_ENDIAN
        guint8  __dummy14__ : 6;
        guint8  rot_ctl_sel : 2;
    #else
        guint8  rot_ctl_sel : 2;
        guint8  __dummy14__ : 6;
    #endif

    guint16 cur_wspeed;

    guint16 wsp_descriptors;
};

#pragma pack()

#endif /* __CDEMUD_MODE_PAGES_H__ */
