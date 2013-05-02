/*
 *  libMirage: MacBinary file filter
 *  Copyright (C) 2013 Henrik Stokseth
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __FILTER_MACBINARY_H__
#define __FILTER_MACBINARY_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "mirage.h"
#include "filter-macbinary-file-filter.h"
#include "../filter-dmg/resource-fork.h"

G_BEGIN_DECLS

typedef enum {
    FF_ISONDESK   = 0x0001,
    FF_COLOR      = 0x000E,
    FF_ISHARED    = 0x0040,
    FF_NOINITS    = 0x0080,
    FF_INITED     = 0x0100,
    FF_CHANGED    = 0x0200,
    FF_CUSTOMICON = 0x0400,
    FF_STATIONARY = 0x0800,
    FF_NAMELOCKED = 0x1000,
    FF_HASBUNDLE  = 0x2000,
    FF_INVISIBLE  = 0x4000,
    FF_ISALIAS    = 0x8000
} finder_flag_t;

typedef enum {
    MC_PROTECTED = 0x01
} macbinary_flag_t;

typedef enum {
    BCEM_UNKNOWN1 = G_MININT8+3,
    BCEM_TERM     = -1,
    BCEM_ZERO     = 0,
    BCEM_RAW      = 2
} bcem_type_t;

typedef enum {
    IMAGE_IMG = 0x0b,
    IMAGE_SMI = 0x0c
} ndif_image_type_t;

#pragma pack(1)
typedef struct
{
    guint8  version;        /* Version (equals zero for v2.0)     */
    guint8  fn_length;      /* Length of filename                 */
    gchar   filename[63];   /* File name (not NUL terminated)     */
    gchar   filetype[4];    /* File type                          */
    gchar   creator[4];     /* File creator                       */
    guint8  finder_flags;   /* Finder flags (MSB)                 */
    guint8  reserved_1;     /* Reserved (always zero)             */
    guint16 vert_pos;       /* Vertical position in window        */
    guint16 horiz_pos;      /* Horizontal position in window      */
    guint16 window_id;      /* Window or folder ID                */
    guint8  flags;          /* File flags                         */
    guint8  reserved_2;     /* Reserved (always zero)             */
    guint32 datafork_len;   /* Data fork length                   */
    guint32 resfork_len;    /* Resource fork length               */
    guint32 created;        /* Creation date                      */
    guint32 modified;       /* Modification date                  */
    guint16 getinfo_len;    /* Length of get info comment         */
    /* The following fields are present only in the v2.0 format   */
    guint8  finder_flags_2; /* Finder flags (LSB)                 */
    guint16 reserved_4[7];  /* Reserved (always zero)             */
    guint32 unpacked_len;   /* Total uncompressed length          */
    guint16 secondary_len;  /* Length of secondary header         */
    guint8  pack_ver;       /* Version used to pack (x-129)       */
    guint8  unpack_ver;     /* Version needed to unpack (x-129)   */
    guint16 crc16;          /* CRC16-XModem of previous 124 bytes */
    guint16 reserved_3;     /* Reserved (always zero)             */
} macbinary_header_t;       /* Length: 128 bytes                  */

typedef struct {
    guint16 imagetype; /* one of ndif_image_type_t */
    guint16 unknown1; /* zero */
    guint8  imagename_len; /* length of imagename */
    gchar   imagename[63]; /* name of image */
    guint32 num_sectors; /* number of sectors in image */
    guint32 unknown2; /* ? */
    guint32 unknown3; /* zero */
    guint32 crc32; /* CRC32 */
    guint32 is_segmented; /* equals one if the image is segmented */
    guint32 unknown4[9]; /* zero */
    guint32 num_blocks; /* Number of bcm_data_t blocks */
} bcem_block_t; /* length: 128 bytes */

typedef struct {
    guint8  sector[3]; /* starting sector */
    gint8   type; /* One of bcem_type_t */
    guint32 offset; /* data fork offset */
    guint32 length; /* data fork length */
} bcem_data_t; /* length: 12 bytes */

typedef struct {
    guint16 part; /* file number in set making up image */
    guint16 parts; /* number of files making up image */
    guint32 unknown1[4]; /* seems to be a constant... */
    guint32 unknown2; /* seems interesting (changes) */
} bcm_block_t; /* length: 24 bytes */
#pragma pack()

G_END_DECLS

#endif /* __FILTER_MACBINARY_H__ */
