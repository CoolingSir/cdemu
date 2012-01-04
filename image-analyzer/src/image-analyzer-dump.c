/*
 *  Image Analyzer: Generic dump functions
 *  Copyright (C) 2007-2012 Rok Mandeljc
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <mirage.h>
#include "image-analyzer-dump.h"


/**********************************************************************\
 *                        Generic dump functions                      *
\**********************************************************************/
gchar *dump_value (gint val, const DUMP_Value *values, gint num_values)
{
    gint i;
    
    for (i = 0; i < num_values; i++) {
        if (values[i].value == val) {
            return values[i].name;
        }
    }
    
    return "<Unknown>";
}

gchar *dump_flags (gint val, const DUMP_Value *values, gint num_values)
{
    static gchar tmp_string[255] = "";
    gchar *ptr = tmp_string;
    gint i;

    memset(tmp_string, 0, sizeof(tmp_string));
        
    for (i = 0; i < num_values; i++) {
        if ((val & values[i].value) == values[i].value) {
            if (strlen(tmp_string)) {
                ptr += sprintf(ptr, "; ");
            }
            ptr += sprintf(ptr, "%s", values[i].name);
        }
    }
    
    return tmp_string;
}


/**********************************************************************\
 *                        Specific dump functions                     *
\**********************************************************************/
gchar *dump_track_flags (gint track_flags)
{
    static DUMP_Value values[] = {
        { MIRAGE_TRACKF_FOURCHANNEL, "four channel audio" },
        { MIRAGE_TRACKF_COPYPERMITTED, "copy permitted" },
        { MIRAGE_TRACKF_PREEMPHASIS, "pre-emphasis" },
    };

    return dump_flags(track_flags, values, G_N_ELEMENTS(values));
}

gchar *dump_track_mode (gint track_mode)
{
    static DUMP_Value values[] = {
        { MIRAGE_MODE_MODE0, "Mode 0" },
        { MIRAGE_MODE_AUDIO, "Audio" },
        { MIRAGE_MODE_MODE1, "Mode 1" },
        { MIRAGE_MODE_MODE2, "Mode 2 Formless" },
        { MIRAGE_MODE_MODE2_FORM1, "Mode 2 Form 1" },
        { MIRAGE_MODE_MODE2_FORM2, "Mode 2 Form 2" },
        { MIRAGE_MODE_MODE2_MIXED, "Mode 2 Mixed" },
    };

    return dump_value(track_mode, values, G_N_ELEMENTS(values));
}

gchar *dump_session_type (gint session_type)
{
    static DUMP_Value values[] = {
        { MIRAGE_SESSION_CD_ROM, "CD-DA/CD-ROM" },
        { MIRAGE_SESSION_CD_I, "CD-I" },
        { MIRAGE_SESSION_CD_ROM_XA, "CD-ROM XA" },
    };

    return dump_value(session_type, values, G_N_ELEMENTS(values));
}

gchar *dump_medium_type (gint medium_type)
{
    static DUMP_Value values[] = {
        { MIRAGE_MEDIUM_CD, "CD-ROM" },
        { MIRAGE_MEDIUM_DVD, "DVD-ROM" },
        { MIRAGE_MEDIUM_BD, "BD Disc" },
    };

    return dump_value(medium_type, values, G_N_ELEMENTS(values));
}


gchar *dump_binary_fragment_tfile_format (gint format)
{
    static DUMP_Value values[] = {
        { FR_BIN_TFILE_DATA, "Binary data" },
        { FR_BIN_TFILE_AUDIO, "Audio data" },
        { FR_BIN_TFILE_AUDIO_SWAP, "Audio data (swapped)" },
    };

    return dump_flags(format, values, G_N_ELEMENTS(values));
}

gchar *dump_binary_fragment_sfile_format (gint format)
{
    static DUMP_Value values[] = {
        { FR_BIN_SFILE_INT, "internal" },
        { FR_BIN_SFILE_EXT, "external" },

        { FR_BIN_SFILE_PW96_INT, "PW96 interleaved" },
        { FR_BIN_SFILE_PW96_LIN, "PW96 linear" },
        { FR_BIN_SFILE_RW96, "RW96" },
        { FR_BIN_SFILE_PQ16, "PQ16" },
    };

    return dump_flags(format, values, G_N_ELEMENTS(values));
}
