/*
 *  libMirage: CCD image parser
 *  Copyright (C) 2006-2008 Rok Mandeljc
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

#ifndef __IMAGE_CCD_H__
#define __IMAGE_CCD_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "mirage.h"

G_BEGIN_DECLS

typedef struct {
    /* [CloneCD] */
    gint Version;
} CCD_CloneCD;

typedef struct {
    /* [Disc] */
    gint TocEntries;
    gint Sessions;
    gint DataTracksScrambled;
    gint CDTextLength;
    
    /* Optional */
    gchar *Catalog;
} CCD_Disc;

typedef struct {
    gint number;
    
    /* [Session] */
    gint PreGapMode;
    gint PreGapSubC;
} CCD_Session;

typedef struct {
    gint number;
        
    /* [Entry] */
    gint Session;
    gint Point;
    gint ADR;
    gint Control;
    gint TrackNo;
    gint AMin;
    gint ASec;
    gint AFrame;
    gint ALBA;
    gint Zero;
    gint PMin;
    gint PSec;
    gint PFrame;
    gint PLBA;
    
    /* [Track] */
    gint Mode;
    gint Index0;
    gint Index1;
    gchar *ISRC;
} CCD_Entry;


GTypeModule *global_module;

G_END_DECLS

#include "image-ccd-parser.h"

#endif /* __IMAGE_CCD_H__ */
