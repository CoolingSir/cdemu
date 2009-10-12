/*
 *  CDEmuD: Packet command definitions
 *  Copyright (C) 2006-2009 Rok Mandeljc
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

#ifndef __CDEMUD_PACKET_COMMANDS_H__
#define __CDEMUD_PACKET_COMMANDS_H__

#define BIG_ENDIAN_BITFIELD    (G_BYTE_ORDER == G_BIG_ENDIAN)
#define LITTLE_ENDIAN_BITFIELD  (G_BYTE_ORDER == G_LITTLE_ENDIAN)

#pragma pack(1)

/* Documents used:
    - INF8090i v7 (Mt. Fuji v.5 rev 1.48)
    - SCSI Multimedia Commands – 3 (MMC-3)
    - SCSI-3 Primary Commands (SPC)
    - SCSI-3 Block Command Set (SBC)
*/

/* ************************************************************************** *\
 *                                   BLANK                                    *
\* ************************************************************************** */
#define PC_BLANK 0xA1

struct BLANK_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  immed       : 1;
        guint8  __dummy2__  : 1;
        guint8  blanking    : 3;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  blanking    : 3;        
        guint8  __dummy2__  : 1;
        guint8  immed       : 1;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 start[4];
    
    guint8  __dummy__[5];
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                CLOSE TRACK                                 *
\* ************************************************************************** */
#define PC_CLOSE_TRACK_SESSION 0x5B

struct CLOSE_TRACK_SESSION_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 7;
        guint8  immed       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  immed       : 1;
        guint8  __dummy1__  : 7;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy2__  : 6;
        guint8  session     : 1;
        guint8  track       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  track       : 1;
        guint8  session     : 1;
        guint8  __dummy2__  : 7;
    #endif
    
    guint8  __dummy3__;
    
    guint8  number[2];
    
    guint8  __dummy4__[3];
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                 ERASE (10)                                 *
\* ************************************************************************** */
#define PC_ERASE_10 0x2C

struct ERASE_10_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 5;
        guint8  era         : 1;
        guint8  __dummy2__  : 1;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy2__  : 1;
        guint8  era         : 1;
        guint8  __dummy1__  : 5;
    #endif
    
    guint8  __dummy3__;
    
    guint16 length;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                FORMAT UNIT                                 *
\* ************************************************************************** */
#define PC_FORMAT_UNIT 0x04

struct FORMAT_UNIT_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  fmt_data    : 1;
        guint8  cmp_list    : 1;
        guint8  fmt_code    : 3;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  fmt_code    : 3;
        guint8  cmp_list    : 1;
        guint8  fmt_data    : 1;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy2__;
    
    guint16 interleave;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                             GET CONFIGURATION                              *
\* ************************************************************************** */
#define PC_GET_CONFIGURATION 0x46

struct GET_CONFIGURATION_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 3;
        guint8  rt          : 2;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  rt          : 2;
        guint8  __dummy2__  : 3;
        guint8  __dummy1__  : 3;
    #endif
    
    guint16 sfn;
    
    guint8  __dummy3__[3];
    
    guint16 length;
    
    guint8  control;
};

struct GET_CONFIGURATION_Header {
    guint32 length;
        
    guint8  __dummy1__[2];
    
    guint16 cur_profile;
};

/* ************************************************************************** *\
 *                        GET EVENT/STATUS NOTIFICATION                       *
\* ************************************************************************** */
#define PC_GET_EVENT_STATUS_NOTIFICATION 0x4A

struct GET_EVENT_STATUS_NOTIFICATION_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 4;
        guint8  immed       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  immed       : 1;
        guint8  __dummy2__  : 4;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy3__[2];
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy5__  : 1;
        guint8  dev_busy    : 1;
        guint8  multi_init  : 1;
        guint8  media       : 1;
        guint8  external    : 1;
        guint8  pwr_man     : 1;
        guint8  op_chg      : 1;
        guint8  __dummy4__  : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy4__  : 1;
        guint8  op_chg      : 1;
        guint8  pwr_man     : 1;
        guint8  external    : 1;
        guint8  media       : 1;
        guint8  multi_init  : 1;
        guint8  dev_busy    : 1;
        guint8  __dummy5__  : 1;
    #endif
    
    guint8  __dummy6__[2];
    
    guint16 length;
    
    guint8  control;
};

struct GET_EVENT_STATUS_NOTIFICATION_Header {
    guint16 length;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  nea         : 1;
        guint8  __dummy1__  : 4;
        guint8  not_class   : 3;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  not_class   : 3;
        guint8  __dummy1__  : 4;
        guint8  nea         : 1;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy5__  : 1;
        guint8  dev_busy    : 1;
        guint8  multi_init  : 1;
        guint8  media       : 1;
        guint8  external    : 1;
        guint8  pwr_man     : 1;
        guint8  op_chg      : 1;
        guint8  __dummy4__  : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy4__  : 1;
        guint8  op_chg      : 1;
        guint8  pwr_man     : 1;
        guint8  external    : 1;
        guint8  media       : 1;
        guint8  multi_init  : 1;
        guint8  dev_busy    : 1;
        guint8  __dummy5__  : 1;
    #endif
};

struct GET_EVENT_STATUS_NOTIFICATION_MediaEventDescriptor {
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 4;
        guint8  event       : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  event       : 4;
        guint8  __dummy1__  : 4;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy2__  : 6;
        guint8  present     : 1;
        guint8  opened      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  opened      : 1;
        guint8  present     : 1;
        guint8  __dummy2__  : 6;
    #endif
    
    guint8  start;
    guint8  end;
};

enum MEDIA_EVENTS {
    MEDIA_EVENT_NOCHANGE = 0,
    MEDIA_EVENT_EJECTREQUEST = 1,
    MEDIA_EVENT_NEW_MEDIA = 2,
    MEDIA_EVENT_MEDIA_REMOVAL = 3,
    MEDIA_EVENT_MEDIA_CHANGE = 4,
};

/* ************************************************************************** *\
 *                              GET PERFORMANCE                               *
\* ************************************************************************** */
#define PC_GET_PERFORMANCE 0xAC

struct GET_PERFORMANCE_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  data_type   : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  data_type   : 5;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 lba[4];
    
    guint8  __dummy2__[2];
    
    guint16 descriptors;
    
    guint8  type;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                  INQUIRY                                   *
\* ************************************************************************** */
#define PC_INQUIRY 0x12

struct INQUIRY_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 6;
        guint8  __dummy2__  : 1;
        guint8  evpd        : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  evpd        : 1;
        guint8  __dummy2__  : 1;
        guint8  __dummy1__  : 6;
    #endif
    
    guint8  page_code;
    
    guint16 length;
    
    guint8  control;
};

struct INQUIRY_Data {
    #if BIG_ENDIAN_BITFIELD
        guint8  per_qual    : 3;
        guint8  per_dev     : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  per_dev     : 3;
        guint8  per_qual    : 5;
    #endif

    #if BIG_ENDIAN_BITFIELD
        guint8  rmb         : 1;
        guint8  __dummy1__  : 7;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy1__  : 7;
        guint8  rmb         : 1;
    #endif
        
    guint8  version;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  atapi_version   : 4;
        guint8  response_fmt    : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  response_fmt    : 4;
        guint8  atapi_version   : 4;
    #endif

    guint8  length;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  sccs        : 1;
        guint8  __dummy3__  : 7;    
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy3__  : 7;    
        guint8  sccs        : 1;
    #endif

    #if BIG_ENDIAN_BITFIELD
        guint8  bque        : 1;
        guint8  encserv     : 1;
        guint8  __dummy4__  : 1;
        guint8  multip      : 1;
        guint8  mchngr      : 1;
        guint8  __dummy5__  : 1;
        guint8  __dummy6__  : 1;
        guint8  addr16      : 1;    
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  addr16      : 1;    
        guint8  __dummy6__  : 1;
        guint8  __dummy5__  : 1;
        guint8  mchngr      : 1;
        guint8  multip      : 1;
        guint8  __dummy4__  : 1;
        guint8  encserv     : 1;
        guint8  bque        : 1;
    #endif

    #if BIG_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy7__  : 1;
        guint8  wbus16      : 1;
        guint8  sync        : 1;
        guint8  linked      : 1;
        guint8  __dummy8__  : 1;
        guint8  cmdque      : 1;
        guint8  __dummy9__  : 1;    
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy9__  : 1;    
        guint8  cmdque      : 1;
        guint8  __dummy8__  : 1;
        guint8  linked      : 1;
        guint8  sync        : 1;
        guint8  wbus16      : 1;
        guint8  __dummy7__  : 1;
        guint8  reladr      : 1;
    #endif

    gchar  vendor_id[8];
    gchar  product_id[16];
    gchar  product_rev[4];
    gchar  vendor_spec1[20];
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy10__ : 4;
        guint8  clocking    : 2;
        guint8  qas         : 1;
        guint8  ius         : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  ius         : 1;
        guint8  qas         : 1;
        guint8  clocking    : 2;
        guint8  __dummy10__ : 4;
    #endif

    guint8  __dummy11__;
    
    guint16 ver_desc1;
    guint16 ver_desc2;
    guint16 ver_desc3;
    guint16 ver_desc4;
    guint16 ver_desc5;
    guint16 ver_desc6;
    guint16 ver_desc7;
    guint16 ver_desc8;
    
    guint8  __dummy12__[22];
};

/* ************************************************************************** *\
 *                             LOAD/UNLOAD MEDIUM                             *
\* ************************************************************************** */
#define PC_LOAD_UNLOAD_MEDUIM 0xA6

struct LOAD_UNLOAD_MEDIUM_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 4;
        guint8  immed       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  immed       : 1;
        guint8  __dummy2__  : 4;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy3__[2];
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy4__  : 6;
        guint8  lo_unlo     : 1;
        guint8  start       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  start       : 1;
        guint8  lo_unlo     : 1;
        guint8  __dummy4__  : 6;
    #endif
    
    guint8  __dummy5__[3];
    
    guint8  slot;
    
    guint8  __dummy6__[2];
    
    guint8  control;
};

/* ************************************************************************** *\
 *                              MECHANISM STATUS                              *
\* ************************************************************************** */
#define PC_MECHANISM_STATUS 0xBD

struct MECHANISM_STATUS_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy3__[6];
    
    guint16 length;
        
    guint8  __dummy4__;
        
    guint8  control;
};

/* ************************************************************************** *\
 *                               MODE SELECT (6)                             *
\* ************************************************************************** */
#define PC_MODE_SELECT_6 0x15

struct MODE_SELECT_6_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  pf          : 1;
        guint8  __dummy2__  : 3;
        guint8  sp          : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  sp          : 1;
        guint8  __dummy2__  : 3;
        guint8  pf          : 1;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy3__[2];
    
    guint8 length;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                               MODE SELECT (10)                             *
\* ************************************************************************** */
#define PC_MODE_SELECT_10 0x55

struct MODE_SELECT_10_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  pf          : 1;
        guint8  __dummy2__  : 3;
        guint8  sp          : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  sp          : 1;
        guint8  __dummy2__  : 3;
        guint8  pf          : 1;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy3__[5];
    
    guint16 length;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                               MODE SENSE (6)                              *
\* ************************************************************************** */
#define PC_MODE_SENSE_6 0x1A

struct MODE_SENSE_6_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 4;
        guint8  dbd         : 1;
        guint8  __dummy2__  : 3;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 3;
        guint8  dbd         : 1;
        guint8  __dummy1__  : 4;
    #endif

    #if BIG_ENDIAN_BITFIELD
        guint8  pc          : 2;
        guint8  page_code   : 6;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  page_code   : 6;
        guint8  pc          : 2;
    #endif
    
    guint8  subpage;
    
    guint8 length;
                
    guint8  control;
};

struct MODE_SENSE_6_Header {
    guint8 length;

    guint8  __dummy1__[2];
    guint8 blkdesc_len;
};

/* ************************************************************************** *\
 *                               MODE SENSE (10)                              *
\* ************************************************************************** */
#define PC_MODE_SENSE_10 0x5A

struct MODE_SENSE_10_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  llbaa       : 1;
        guint8  dbd         : 1;
        guint8  __dummy2__  : 3;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 3;
        guint8  dbd         : 1;
        guint8  llbaa       : 1;
        guint8  __dummy1__  : 3;
    #endif

    #if BIG_ENDIAN_BITFIELD
        guint8  pc          : 2;
        guint8  page_code   : 6;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  page_code   : 6;
        guint8  pc          : 2;
    #endif
    
    guint8  subpage;
    
    guint8  __dummy3__[3];

    guint16 length;
                
    guint8  control;
};


struct MODE_SENSE_10_Header {
    guint16 length;

    guint8  __dummy1__[4];
    guint16 blkdesc_len;
};

/* ************************************************************************** *\
 *                                PAUSE/RESUME                                *
\* ************************************************************************** */
#define PC_PAUSE_RESUME 0x4B

struct PAUSE_RESUME_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy3__[6];

    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy4__  : 7;
        guint8  resume      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  resume      : 1;
        guint8  __dummy4__  : 7;
    #endif
    
    guint8  control;
};

/* ************************************************************************** *\
 *                               PLAY AUDIO (10)                              *
\* ************************************************************************** */
#define PC_PLAY_AUDIO_10 0x45

struct PLAY_AUDIO_10_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 4;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy2__  : 4;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 lba;

    guint8  __dummy3__;

    guint16 play_len;

    guint8  control;
};

/* ************************************************************************** *\
 *                               PLAY AUDIO (12)                              *
\* ************************************************************************** */
#define PC_PLAY_AUDIO_12 0xA5

struct PLAY_AUDIO_12_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 4;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy2__  : 4;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 lba;

    guint32 play_len;

    guint8  __dummy3__;

    guint8  control;
};

/* ************************************************************************** *\
 *                               PLAY AUDIO MSF                               *
\* ************************************************************************** */
#define PC_PLAY_AUDIO_MSF 0x47

struct PLAY_AUDIO_MSF_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy3__;
    
    guint8  start_m;
    guint8  start_s;
    guint8  start_f;
    
    guint8  end_m;
    guint8  end_s;
    guint8  end_f;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                         PREVENT/ALLOW MEDIUM REMOVAL                       *
\* ************************************************************************** */
#define PC_PREVENT_ALLOW_MEDIUM_REMOVAL 0x1E

struct PREVENT_ALLOW_MEDIUM_REMOVAL_CDB {
    guint8  code;

    guint8  __dummy1__[3];
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy2__  : 6;
        guint8  prevent     : 2;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  prevent     : 2;
        guint8  __dummy2__  : 6;
    #endif
        
    guint8  control;
};

/* ************************************************************************** *\
 *                                  READ (10)                                 *
\* ************************************************************************** */
#define PC_READ_10 0x28

struct READ_10_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  dpo         : 1;
        guint8  fua         : 1;
        guint8  __dummy2__  : 2;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy2__  : 2;
        guint8  fua         : 1;
        guint8  dpo         : 1;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 lba;
    
    guint8  __dummy3__;
    
    guint16 length;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                  READ (12)                                 *
\* ************************************************************************** */
#define PC_READ_12 0xA8

struct READ_12_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  dpo         : 1;
        guint8  fua         : 1;
        guint8  __dummy2__  : 2;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy2__  : 2;
        guint8  fua         : 1;
        guint8  dpo         : 1;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 lba;
    
    guint32 length;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  stream      : 1;
        guint8  __dummy3__  : 7;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy3__  : 7;
        guint8  stream      : 1;
    #endif
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                 READ BUFFER                                *
\* ************************************************************************** */
#define PC_READ_BUFFER 0x3C

struct READ_BUFFER_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  mode        : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  mode        : 5;
        guint8  __dummy1__  : 3;
    #endif
    
    /* life sucks, 3 bytes field suck... */
    #if BIG_ENDIAN_BITFIELD
        guint32 buf_id  : 8;
    #elif LITTLE_ENDIAN_BITFIELD
        guint32 buf_id  : 8;
    #endif
    guint32 buf_off : 24;
    
    guint32 length  : 24;
    #if BIG_ENDIAN_BITFIELD
        guint32 control : 8;
    #elif LITTLE_ENDIAN_BITFIELD
        guint32 control : 8;
    #endif
};

/* ************************************************************************** *\
 *                             READ BUFFER CAPACITY                           *
\* ************************************************************************** */
#define PC_READ_BUFFER_CAPACITY 0x5C

struct READ_BUFFER_CAPACITY_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 4;
        guint8  block       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  block       : 1;
        guint8  __dummy2__  : 4;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy3__[5];
        
    guint32 length;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                READ CAPACITY                               *
\* ************************************************************************** */
#define PC_READ_CAPACITY 0x25

struct READ_CAPACITY_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 4;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy2__  : 4;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 lba;
    
    guint8  __dummy3__[2];
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy4__  : 7;
        guint8  pmi         : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  pmi         : 1;
        guint8  __dummy4__  : 7;
    #endif
        
    guint8  control;
};

struct READ_CAPACITY_Data {
    guint32 lba;
    guint32 block_size;
};

/* ************************************************************************** *\
 *                                   READ CD                                  *
\* ************************************************************************** */
#define PC_READ_CD 0xBE

struct READ_CD_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  sect_type   : 3;
        guint8  __dummy2__  : 1;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy2__  : 1;
        guint8  sect_type   : 3;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 lba;
        
    guint32 length  : 24;
    
    guint8  byte9; /* MCSB */
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy4__  : 5;
        guint8  subchan     : 3;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  subchan     : 3;
        guint8  __dummy4__  : 5;
    #endif
        
    guint8  control;
};


/* ************************************************************************** *\
 *                                 READ CD MSF                                *
\* ************************************************************************** */
#define PC_READ_CD_MSF 0xB9

struct READ_CD_MSF_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  sect_type   : 3;
        guint8  __dummy2__  : 2;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 2;
        guint8  sect_type   : 3;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy3__;
        
    guint8  start_m;
    guint8  start_s;
    guint8  start_f;

    guint8  end_m;
    guint8  end_s;
    guint8  end_f;

    guint8  byte9; /* MCSB */
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy5__  : 5;
        guint8  subchan     : 3;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  subchan     : 3;
        guint8  __dummy5__  : 5;
    #endif
        
    guint8  control;
};

/* ************************************************************************** *\
 *                             READ DISC INFORMATION                          *
\* ************************************************************************** */
#define PC_READ_DISC_INFORMATION 0x51

struct READ_DISC_INFORMATION_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 5;
        guint8  type        : 3;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  type        : 3;
        guint8  __dummy1__  : 5;
    #endif
    
    guint8  __dummy2__[5];
    
    guint16 length;
        
    guint8  control;
};

struct READ_DISC_INFORMATION_Data {
    guint16    length;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  erasable    : 1;
        guint8  lsession_state  : 2;
        guint8  disc_status : 2;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  disc_status : 2;
        guint8  lsession_state  : 2;
        guint8  erasable    : 1;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  ftrack_disc;
    guint8  sessions1;
    guint8  ftrack_lsession1;
    guint8  ltrack_lsession1;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  did_v       : 1;
        guint8  dbc_v       : 1;
        guint8  uru         : 1;
        guint8  __dummy2__  : 2;
        guint8  dbit        : 1;
        guint8  bg_fmt      : 2;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  bg_fmt      : 2;
        guint8  dbit        : 1;
        guint8  __dummy2__  : 2;
        guint8  uru         : 1;
        guint8  dbc_v       : 1;
        guint8  did_v       : 1;
    #endif

    guint8  disc_type;
    
    guint8  sessions0;
    guint8  ftrack_lsession0;
    guint8  ltrack_lsession0;
    
    guint32 disc_id;
    guint32 lsession_leadin;
    guint32 last_leadout;
    guint8  bar_code[8];
    guint8  __dummy3__;
    guint8  opc_entries;
};

/* ************************************************************************** *\
 *                              READ DVD STRUCTURE                            *
\* ************************************************************************** */
#define PC_READ_DVD_STRUCTURE 0xAD

struct READ_DVD_STRUCTURE_CDB {
    guint8  code;
        
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 lba;
    
    guint8  layer;
    
    guint8  format;
    
    guint16 length;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  agid        : 2;
        guint8  __dummy3__  : 6;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy3__  : 6;
        guint8  agid        : 2;
    #endif
    
    guint8  control;
};

struct READ_DVD_STRUCTURE_Header {
    guint16 length;
    guint8  __dummy1__;
    guint8  __dummy2__;
};

struct READ_DVD_STRUCTURE_Format_0x0000_Descriptor {
    #if BIG_ENDIAN_BITFIELD
        guint8  book_type   : 4;
        guint8  part_ver    : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  part_ver    : 4;
        guint8  book_type   : 4;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint8  disc_size   : 4;
        guint8  max_rate    : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  max_rate    : 4;
        guint8  disc_size   : 4;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 1;
        guint8  num_layers  : 2;
        guint8  track_path  : 1;
        guint8  layer_type  : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  layer_type  : 4;
        guint8  track_path  : 1;
        guint8  num_layers  : 2;
        guint8  __dummy1__  : 1;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint8  linear_density  : 4;
        guint8  track_density   : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  track_density   : 4;
        guint8  linear_density  : 4;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint32 __dummy2__      : 8;
        guint32 data_start      : 24;
    #elif LITTLE_ENDIAN_BITFIELD
        guint32 data_start      : 24;
        guint32 __dummy2__      : 8;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint32 __dummy3__      : 8;
        guint32 data_end        : 24;
    #elif LITTLE_ENDIAN_BITFIELD
        guint32 data_end        : 24;
        guint32 __dummy3__      : 8;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint32 __dummy4__      : 8;
        guint32 layer0_end      : 24;
    #elif LITTLE_ENDIAN_BITFIELD
        guint32 layer0_end      : 24;
        guint32 __dummy4__      : 8;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint8  bca             : 1;
        guint8  __dummy5__      : 7;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy5__      : 7;
        guint8  bca             : 1;
    #endif
    
    guint8 media_specific[2031];
};

struct READ_DVD_STRUCTURE_Format_0x0001_Descriptor {
   guint8   copy_protection;
   guint8   region_info;
   guint8   __dummy1__;
   guint8   __dummy2__;    
};

struct READ_DVD_STRUCTURE_Format_0x0004_Descriptor {
    guint8  disc_manufacturing_data[2048];
};

/* ************************************************************************** *\
 *                             READ FORMAT CAPACITIES                         *
\* ************************************************************************** */
#define PC_READ_FORMAT_CAPACITIES 0x23 

struct READ_FORMAT_CAPACITIES_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
    
    guint8  __dummy3__[5];
        
    guint16 length;
        
    guint8  control;
};

/* ************************************************************************** *\
 *                               READ SUB-CHANNEL                             *
\* ************************************************************************** */
#define PC_READ_SUBCHANNEL 0x42

struct READ_SUBCHANNEL_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 3;
        guint8  time        : 1;
        guint8  __dummy3__  : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy3__  : 1;
        guint8  time        : 1;
        guint8  __dummy2__  : 3;
        guint8  __dummy1__  : 3;
    #endif

    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy4__  : 1;
        guint8  subq        : 1;
        guint8  __dummy5__  : 6;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy5__  : 6;
        guint8  subq        : 1;
        guint8  __dummy4__  : 1;
    #endif
    
    guint8  param_list;
    
    guint8  __dummy6__[2];
    
    guint8  track;
        
    guint16 length;
        
    guint8  control;
};

struct READ_SUBCHANNEL_Header {
    guint8  __dummy1__;
    guint8  audio_status;
    guint16 length;
};

enum AUDIO_STATUS {
    AUDIO_STATUS_UNSUPPORTED    = 0x00,
    AUDIO_STATUS_PLAYING        = 0x11,
    AUDIO_STATUS_PAUSED         = 0x12,
    AUDIO_STATUS_COMPLETED      = 0x13,
    AUDIO_STATUS_ERROR          = 0x14,
    AUDIO_STATUS_NOSTATUS       = 0x15
};

struct READ_SUBCHANNEL_Data1 {
    guint8  fmt_code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  adr         : 4;
        guint8  ctl         : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  ctl         : 4;
        guint8  adr         : 4;
    #endif
    
    guint8  track;
    guint8  index;
    
    guint32 abs_addr;
    guint32 rel_addr;
};

struct READ_SUBCHANNEL_Data2 {
    guint8  fmt_code;
    guint8  __dummy1__[3];
    
    #if BIG_ENDIAN_BITFIELD
        guint8  mcval       : 1;
        guint8  __dummy2__  : 7;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 7;
        guint8  mcval       : 1;
    #endif
    
    guint8  mcn[13];
    guint8  zero;
    guint8  aframe;
};

struct READ_SUBCHANNEL_Data3 {
    guint8  fmt_code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  adr         : 4;
        guint8  ctl         : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  ctl         : 4;
        guint8  adr         : 4;
    #endif
    
    guint8  track;
    
    guint8  __dummy1__;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  tcval       : 1;
        guint8  __dummy2__  : 7;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 7;
        guint8  tcval       : 1;
    #endif

    guint8  isrc[12];
    guint8  zero;
    guint8  aframe;
    guint8  __dummy3__;
};

/* ************************************************************************** *\
 *                               READ TOC/PMA/ATIP                            *
\* ************************************************************************** */
#define PC_READ_TOC_PMA_ATIP 0x43

struct READ_TOC_PMA_ATIP_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 3;
        guint8  time        : 1;
        guint8  __dummy3__  : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy3__  : 1;
        guint8  time        : 1;
        guint8  __dummy2__  : 3;
        guint8  __dummy1__  : 3;
    #endif

    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy4__  : 4;
        guint8  format      : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  format      : 4;
        guint8  __dummy4__  : 4;
    #endif
    
    guint8  __dummy5__[3];
        
    guint8  number;
        
    guint16 length;
        
    guint8  control;
};

struct READ_TOC_PMA_ATIP_0000_Header {
    guint16 length;
    guint8  ftrack;
    guint8  ltrack;
};

struct READ_TOC_PMA_ATIP_0000_Descriptor {
    guint8  __dummy1__;

    #if BIG_ENDIAN_BITFIELD
        guint8  adr         : 4;
        guint8  ctl         : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  ctl         : 4;
        guint8  adr         : 4;
    #endif

    guint8  number;
    
    guint8  __dummy2__;
    
    guint32 lba;
};

struct READ_TOC_PMA_ATIP_0001_Data {
    guint16 length;
    guint8  fsession;
    guint8  lsession;
    
    guint8  __dummy1__;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  adr         : 4;
        guint8  ctl         : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  ctl         : 4;
        guint8  adr         : 4;
    #endif
    
    guint8  ftrack;
    
    guint8  __dummy2__;
    
    guint32 lba;
};

struct READ_TOC_PMA_ATIP_0010_Header {
    guint16 length;
    guint8  fsession;
    guint8  lsession;
} ;

struct READ_TOC_PMA_ATIP_0010_Descriptor {
    guint8  session;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  adr         : 4;
        guint8  ctl         : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  ctl         : 4;
        guint8  adr         : 4;
    #endif
    
    guint8  tno;
    guint8  point;
    guint8  min;
    guint8  sec;
    guint8  frame;
    guint8  zero;
    guint8  pmin;
    guint8  psec;
    guint8  pframe;
} ;

struct READ_TOC_PMA_ATIP_0100_Header {
    guint16 length;
    guint8  __dummy1__[2];
};

struct READ_TOC_PMA_ATIP_0101_Header {
    guint16 length;
    guint8  __dummy1__[2];
};

/* ************************************************************************** *\
 *                             READ TRACK INFORMATION                         *
\* ************************************************************************** */
#define PC_READ_TRACK_INFORMATION 0x52

struct READ_TRACK_INFORMATION_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 5;
        guint8  open        : 1;
        guint8  type        : 2;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  type        : 2;
        guint8  open        : 1;
        guint8  __dummy1__  : 5;
    #endif
    
    guint32 number;

    guint8  __dummy2__;

    guint16 length;
        
    guint8  control;
};

struct READ_TRACK_INFORMATION_Data {
    guint16 length;
    
    guint8  track_number1;
    guint8  session_number1;
    
    guint8  __dummy1__;

    #if BIG_ENDIAN_BITFIELD
        guint8  ljrs        : 2;
        guint8  damage      : 1;
        guint8  copy        : 1;
        guint8  track_mode  : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  track_mode  : 4;
        guint8  copy        : 1;
        guint8  damage      : 1;
        guint8  ljrs        : 2;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint8  rt          : 1;
        guint8  blank       : 1;
        guint8  packet      : 1;
        guint8  fp          : 1;
        guint8  data_mode   : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  data_mode   : 4;
        guint8  fp          : 1;
        guint8  packet      : 1;
        guint8  blank       : 1;
        guint8  rt          : 1;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy3__  : 6;
        guint8  lra_v       : 1;
        guint8  nwa_v       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  nwa_v       : 1;
        guint8  lra_v       : 1;
        guint8  __dummy3__  : 6;
    #endif

    guint32 start_address;
    guint32 next_writeable_address;
    guint32 free_blocks;
    guint32 fixed_packet_size;
    guint32 track_size;
    guint32 last_recorded_address;
    
    guint8  track_number0;
    guint8  session_number0;
    
    guint8  __dummy4__[2];
    
    guint32 read_compatibility_lba;
    
    guint32 next_layer_jump_address;
    guint32 last_layer_jump_address;
};

/* ************************************************************************** *\
 *                                 REPAIR TRACK                               *
\* ************************************************************************** */
#define PC_REPAIR_TRACK 0x58

struct REPAIR_TRACK_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 7;
        guint8  immed       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  immed       : 1;
        guint8  __dummy1__  : 7;
    #endif
    
    guint8  __dummy2__[2];

    guint16 track;

    guint8  __dummy3__[3];
        
    guint8  control;
};

/* ************************************************************************** *\
 *                                 REPORT KEY                                 *
\* ************************************************************************** */
#define PC_REPORT_KEY 0xA4

struct REPORT_KEY_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 lba;

    guint8  __dummy3__;
    
    guint8  key_class;

    guint16 length;

    #if BIG_ENDIAN_BITFIELD
        guint8  agid        : 2;
        guint8  key_format  : 6;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  key_format  : 6;
        guint8  agid        : 2;
    #endif
    
    guint8  control;
};

struct REPORT_KEY_001000_Data {
    guint16 length;
    guint8  __dummy1__;
    guint8  __dummy2__;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  type_code   : 2;
        guint8  vendor_resets   : 3;
        guint8  user_changes    : 3;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  user_changes    : 3;
        guint8  vendor_resets   : 3;
        guint8  type_code   : 2;
    #endif
    
    guint8  region_mask;
    guint8  rpc_scheme;
    guint8  __dummy3__;
};
/* ************************************************************************** *\
 *                                REQUEST SENSE                               *
\* ************************************************************************** */
#define PC_REQUEST_SENSE 0x03 

struct REQUEST_SENSE_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 7;
        guint8  desc        : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  desc        : 1;
        guint8  __dummy1__  : 7;
    #endif
    
    guint8  __dummy2__[2];
    
    guint8  length;

    guint8  control;
};

struct REQUEST_SENSE_SenseFixed {
    #if BIG_ENDIAN_BITFIELD
        guint8  valid       : 1;
        guint8  res_code    : 7;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  res_code    : 7;
        guint8  valid       : 1;
    #endif
    
    guint8  __dummy1__;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  file_mark   : 1;
        guint8  eom         : 1;
        guint8  ili         : 1;
        guint8  __dummy2__  : 1;
        guint8  sense_key   : 4;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  sense_key   : 4;
        guint8  __dummy2__  : 1;
        guint8  ili         : 1;
        guint8  eom         : 1;
        guint8  file_mark   : 1;
    #endif
    
    guint8  info[4];
    
    guint8  length;

    guint8  cmd_info[4];
    
    guint8  asc;
    guint8  ascq;
    guint8  fruc;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  sksv        : 1;
        guint8  sk_spec0    : 7;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  sk_spec0    : 7;  
        guint8  sksv        : 1;
    #endif
    guint8  sk_spec1;
    guint8  sk_spec2;
};

/* ************************************************************************** *\
 *                                RESERVE TRACK                               *
\* ************************************************************************** */
#define PC_RESERVE_TRACK 0x53

struct RESERVE_TRACK_CDB {
    guint8  code;

    guint8  __dummy1__[4];
    
    guint32 size;

    guint8  control;
};

/* ************************************************************************** *\
 *                                    SCAN                                    *
\* ************************************************************************** */
#define PC_SCAN 0xBA

struct SCAN_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  direct      : 1;
        guint8  __dummy2__  : 3;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy2__  : 3;
        guint8  direct      : 1;
        guint8  __dummy1__  : 3;
    #endif
    
    guint32 lba;
    
    guint8  __dummy3__[3];
    
    #if BIG_ENDIAN_BITFIELD
        guint8  type        : 2;
        guint8  __dummy4__  : 6;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy4__  : 6;
        guint8  type        : 2;
    #endif
    
    guint8  __dummy5__;

    guint8  control;
};

/* ************************************************************************** *\
 *                                  SEEK (10)                                 *
\* ************************************************************************** */
#define PC_SEEK_10 0x2B

struct SEEK_10_CDB {
    guint8  code;

    guint8  __dummy1__;
    
    guint32 lba;
    
    guint8  __dummy2__[3];
    
    guint8  control;
};

/* ************************************************************************** *\
 *                               SEND CUE SHEET                               *
\* ************************************************************************** */
#define PC_SEND_CUE_SHEET 0x5D 

struct SEND_CUE_SHEET_CDB {
    guint8  code;

    guint8  __dummy1__[5];
    
    guint8  length[3];
        
    guint8  control;
};

/* ************************************************************************** *\
 *                             SEND DVD STRUCTURE                             *
\* ************************************************************************** */
#define PC_SEND_DVD_STRUCTURE 0xBF

struct SEND_DVD_STRUCTURE_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
        
    guint8  __dummy3__[5];
    
    guint8  format;
    
    guint16 struct_data;
    
    guint8  __dummy4__;

    guint8  control;
};

/* ************************************************************************** *\
 *                                 SEND EVENT                                 *
\* ************************************************************************** */
#define PC_SEND_EVENT 0xA2

struct SEND_EVENT_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 4;
        guint8  immed       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  immed       : 1;
        guint8  __dummy2__  : 4;
        guint8  __dummy1__  : 3;
    #endif
        
    guint8  __dummy3__[6];
        
    guint16 length;
    
    guint8  __dummy4__;

    guint8  control;
};

/* ************************************************************************** *\
 *                                  SEND KEY                                  *
\* ************************************************************************** */
#define PC_SEND_KEY 0xA3

struct SEND_KEY_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
        
    guint8  __dummy3__[6];
        
    guint16 length;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  agid        : 2;
        guint8  key_fmt     : 6;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  key_fmt     : 6;
        guint8  agid        : 2;
    #endif
    
    guint8  control;
};

/* ************************************************************************** *\
 *                            SEND OPC INFORMATION                            *
\* ************************************************************************** */
#define PC_SEND_OPC_INFORMATION 0x54

struct SEND_OPC_INFORMATION_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 4;
        guint8  do_opc      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  do_opc      : 1;
        guint8  __dummy2__  : 4;
        guint8  __dummy1__  : 3;
    #endif
        
    guint8  __dummy3__[5];
        
    guint16 length;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                SET CD SPEED                                *
\* ************************************************************************** */
#define PC_SET_CD_SPEED 0xBB

struct SET_CD_SPEED_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 6;
        guint8  rot_ctl     : 2;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  rot_ctl     : 2;
        guint8  __dummy1__  : 6;
    #endif
        
    guint16 read_speed;
    
    guint16 write_speed;
        
    guint8  __dummy2__[5];
    
    guint8  control;
};

/* ************************************************************************** *\
 *                               SET READ AHEAD                               *
\* ************************************************************************** */
#define PC_SET_READ_AHEAD 0xA7

struct SET_READ_AHEAD_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
        
    guint32 trigger_lba;
    
    guint32 ra_lba;
    
    guint8  __dummy3__;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                               SET STREAMING                                *
\* ************************************************************************** */
#define PC_SET_STREAMING 0xB6

struct SET_STREAMING_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
        
    guint8  __dummy3__[7];
    
    guint16 length;
        
    guint8  control;
};

/* ************************************************************************** *\
 *                              START/STOP UNIT                               *
\* ************************************************************************** */
#define PC_START_STOP_UNIT 0x1B

struct START_STOP_UNIT_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 7;
        guint8  immed       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  immed       : 1;
        guint8  __dummy1__  : 7;
    #endif
        
    guint8  __dummy2__[2];
    
    #if BIG_ENDIAN_BITFIELD
        guint8  pwr_cond    : 4;
        guint8  __dummy3__  : 2;
        guint8  lo_ej       : 1;
        guint8  start       : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  start       : 1;
        guint8  lo_ej       : 1;
        guint8  __dummy3__  : 2;
        guint8  pwr_cond    : 4;
    #endif
    
    guint8  control;
};

/* ************************************************************************** *\
 *                              STOP/PLAY SCAN                                *
\* ************************************************************************** */
#define PC_STOP_PLAY_SCAN 0x4E

struct STOP_PLAY_SCAN_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy2__  : 5;
        guint8  __dummy1__  : 3;
    #endif
        
    guint8  __dummy3__[7];
    
    guint8  control;
};

/* ************************************************************************** *\
 *                             SYNCHRONIZE CACHE                              *
\* ************************************************************************** */
#define PC_SYNCHRONIZE_CACHE 0x35

struct SYNCHRONIZE_CACHE_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  __dummy2__  : 3;
        guint8  immed       : 1;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  immed       : 1;
        guint8  __dummy2__  : 3;
        guint8  __dummy1__  : 3;
    #endif
        
    guint32 lba;
    
    guint8  __dummy3__;

    guint16 blocks;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                              TEST UNIT READY                               *
\* ************************************************************************** */
#define PC_TEST_UNIT_READY 0x00

struct TEST_UNIT_READY_CDB {
    guint8  code;
    
    guint8  __dummy1__[4];
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                VERIFY (10)                                 *
\* ************************************************************************** */
#define PC_VERIFY_10 0x2F

struct VERIFY_10_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  dpo         : 1;
        guint8  __dummy2__  : 1;
        guint8  __dummy3__  : 1;
        guint8  bytchk      : 1;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  bytchk      : 1;
        guint8  __dummy3__  : 1;
        guint8  __dummy2__  : 1;
        guint8  dpo         : 1;
        guint8  __dummy1__  : 3;
    #endif
        
    guint32 lba;

    guint8  __dummy4__;
    
    guint16 length;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                 WRITE (10)                                 *
\* ************************************************************************** */
#define PC_WRITE_10 0x2A

struct WRITE_10_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  dpo         : 1;
        guint8  fua         : 1;
        guint8  __dummy2__  : 2;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy2__  : 2;
        guint8  fua         : 1;
        guint8  dpo         : 1;
        guint8  __dummy1__  : 3;
    #endif
        
    guint32 lba;
    
    guint8  __dummy3__;

    guint16 length;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                 WRITE (12)                                 *
\* ************************************************************************** */
#define PC_WRITE_12 0xAA

struct WRITE_12_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  dpo         : 1;
        guint8  fua         : 1;
        guint8  ebp         : 1;
        guint8  __dummy2__  : 1;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  __dummy2__  : 1;
        guint8  ebp         : 1;
        guint8  fua         : 1;
        guint8  dpo         : 1;
        guint8  __dummy1__  : 3;
    #endif
        
    guint32 lba;
    
    guint32 length;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  stream      : 1;
        guint8  __dummy3__  : 7;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  __dummy3__  : 7;
        guint8  stream      : 1;
    #endif
    
    guint8  control;
};

/* ************************************************************************** *\
 *                            WRITE AND VERIFY (10)                           *
\* ************************************************************************** */
#define PC_WRITE_AND_VERIFY_10 0x2E

struct WRITE_AND_VERIFY_10_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  dpo         : 1;
        guint8  __dummy2__  : 2;
        guint8  bytchk      : 1;
        guint8  reladr      : 1;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  reladr      : 1;
        guint8  bytchk      : 1;
        guint8  __dummy2__  : 2;
        guint8  dpo         : 1;
        guint8  __dummy1__  : 3;
    #endif
        
    guint32 lba;
    
    guint32 length;
    
    guint8  __dummy3__;
    
    guint8  control;
};

/* ************************************************************************** *\
 *                                WRITE BUFFER                                *
\* ************************************************************************** */
#define PC_WRITE_BUFFER 0x3B

struct WRITE_BUFFER_CDB {
    guint8  code;
    
    #if BIG_ENDIAN_BITFIELD
        guint8  __dummy1__  : 3;
        guint8  mode        : 5;
    #elif LITTLE_ENDIAN_BITFIELD
        guint8  mode        : 5;
        guint8  __dummy1__  : 3;
    #endif
    
    #if BIG_ENDIAN_BITFIELD
        guint32 buf_id      : 8;
    #elif LITTLE_ENDIAN_BITFIELD
        guint32 buf_id      : 8;
    #endif
    guint32 buf_off : 24;
    
    #if BIG_ENDIAN_BITFIELD
        guint32 length      : 24;
    #elif LITTLE_ENDIAN_BITFIELD
        guint32 length      : 24;
    #endif
    guint32 control : 8;
};

#pragma pack()

#endif /* __CDEMUD_PACKET_COMMANDS_H__ */
