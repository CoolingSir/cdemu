/*
 *  Image Analyzer: Generic dump functions
 *  Copyright (C) 2007-2014 Rok Mandeljc
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <mirage.h>
#include "dump.h"


/**********************************************************************\
 *                        Generic dump functions                      *
\**********************************************************************/
gchar *dump_value (gint val, const DumpValue *values, gint num_values)
{
    for (gint i = 0; i < num_values; i++) {
        if (values[i].value == val) {
            return values[i].name;
        }
    }

    return "<Unknown>";
}

gchar *dump_flags (gint val, const DumpValue *values, gint num_values)
{
    static gchar tmp_string[256] = "";
    gchar *ptr = tmp_string;

    memset(tmp_string, 0, sizeof(tmp_string));

    for (gint i = 0; i < num_values; i++) {
        if ((val & values[i].value) == values[i].value) {
            if (strlen(tmp_string) > 0) {
                g_assert(strlen(tmp_string)+3 <= sizeof(tmp_string));
                ptr += g_snprintf(ptr, 3, "; ");
            }
            g_assert(strlen(values[i].name)+1 <= sizeof(tmp_string));
            ptr += g_snprintf(ptr, strlen(values[i].name)+1, "%s", values[i].name);
        }
    }

    return tmp_string;
}


/**********************************************************************\
 *                        Specific dump functions                     *
\**********************************************************************/
gchar *dump_track_flags (gint track_flags)
{
    static DumpValue values[] = {
        { MIRAGE_TRACK_FLAG_FOURCHANNEL, "four channel audio" },
        { MIRAGE_TRACK_FLAG_COPYPERMITTED, "copy permitted" },
        { MIRAGE_TRACK_FLAG_PREEMPHASIS, "pre-emphasis" },
    };

    return dump_flags(track_flags, values, G_N_ELEMENTS(values));
}

gchar *dump_track_sector_type (gint track_mode)
{
    static DumpValue values[] = {
        { MIRAGE_SECTOR_MODE0, "Mode 0" },
        { MIRAGE_SECTOR_AUDIO, "Audio" },
        { MIRAGE_SECTOR_MODE1, "Mode 1" },
        { MIRAGE_SECTOR_MODE2, "Mode 2 Formless" },
        { MIRAGE_SECTOR_MODE2_FORM1, "Mode 2 Form 1" },
        { MIRAGE_SECTOR_MODE2_FORM2, "Mode 2 Form 2" },
        { MIRAGE_SECTOR_MODE2_MIXED, "Mode 2 Mixed" },
    };

    return dump_value(track_mode, values, G_N_ELEMENTS(values));
}

gchar *dump_session_type (gint session_type)
{
    static DumpValue values[] = {
        { MIRAGE_SESSION_CDROM, "CD-DA/CD-ROM" },
        { MIRAGE_SESSION_CDI, "CD-I" },
        { MIRAGE_SESSION_CDROM_XA, "CD-ROM XA" },
    };

    return dump_value(session_type, values, G_N_ELEMENTS(values));
}

gchar *dump_medium_type (gint medium_type)
{
    static DumpValue values[] = {
        { MIRAGE_MEDIUM_CD, "CD-ROM" },
        { MIRAGE_MEDIUM_DVD, "DVD-ROM" },
        { MIRAGE_MEDIUM_BD, "BlueRay Disc" },
        { MIRAGE_MEDIUM_HD, "HD-DVD Disc" },
        { MIRAGE_MEDIUM_HDD, "Hard-disk" }
    };

    return dump_value(medium_type, values, G_N_ELEMENTS(values));
}


gchar *dump_binary_fragment_main_format (gint format)
{
    static DumpValue values[] = {
        { MIRAGE_MAIN_DATA_FORMAT_DATA, "Binary data" },
        { MIRAGE_MAIN_DATA_FORMAT_AUDIO, "Audio data" },
        { MIRAGE_MAIN_DATA_FORMAT_AUDIO_SWAP, "Audio data (swapped)" },
    };

    return dump_flags(format, values, G_N_ELEMENTS(values));
}

gchar *dump_binary_fragment_subchannel_format (gint format)
{
    static DumpValue values[] = {
        { MIRAGE_SUBCHANNEL_DATA_FORMAT_INTERNAL, "internal" },
        { MIRAGE_SUBCHANNEL_DATA_FORMAT_EXTERNAL, "external" },

        { MIRAGE_SUBCHANNEL_DATA_FORMAT_PW96_INTERLEAVED, "PW96 interleaved" },
        { MIRAGE_SUBCHANNEL_DATA_FORMAT_PW96_LINEAR, "PW96 linear" },
        { MIRAGE_SUBCHANNEL_DATA_FORMAT_RW96, "RW96" },
        { MIRAGE_SUBCHANNEL_DATA_FORMAT_Q16, "Q16" },
    };

    return dump_flags(format, values, G_N_ELEMENTS(values));
}
