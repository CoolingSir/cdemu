/*
 *  libMirage: CIF image parser: Parser object
 *  Copyright (C) 2008-2012 Henrik Stokseth
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

#include "image-cif.h"

#define __debug__ "CIF-Parser"


static const guint8 riff_signature[4] = { 'R', 'I', 'F', 'F' };
static const guint8 imag_signature[4] = { 'I', 'M', 'A', 'G' };
static const guint8  ofs_signature[4] = { 'o', 'f', 's', ' ' };
static const guint8 disc_signature[4] = { 'd', 'i', 's', 'c' };


/**********************************************************************\
 *                          Private structure                         *
\**********************************************************************/
#define MIRAGE_PARSER_CIF_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), MIRAGE_TYPE_PARSER_CIF, MirageParserCifPrivate))

struct _MirageParserCifPrivate
{
    MirageDisc *disc;

    GInputStream *cif_stream;

    /* "disc" block offset and length */
    guint64 disc_offset;
    guint32 disc_length;

    /* "ofs " block offset and length */
    guint64 ofs_offset;
    guint32 ofs_length;

    /* Parsed offset entries */
    gint num_offset_entries;
    CIF_OffsetEntry *offset_entries;

    gint track_counter;

    gint prev_track_mode;
};


/**********************************************************************\
 *                           Debug helpers                            *
\**********************************************************************/
static inline const gchar *debug_image_type (CIF_Image type)
{
    switch (type) {
        case DATA: return "Data CD";
        case MIXED: return "MixedMode CD";
        case MUSIC: return "Music CD";
        case ENCHANCED: return "Enchanced CD";
        case VIDEO: return "Video CD";
        case BOOTABLE: return "Bootable CD";
        case MP3: return "MP3 CD";
        default: return "UNKNOWN";
    }
}

static inline const gchar *debug_session_type (CIF_Session type)
{
    switch (type) {
        case CDDA: return "CD-DA";
        case CDROM: return "CD-ROM";
        case CDROMXA: return "CD-ROM XA";
        default: return "UNKNOWN";
    }
}

static inline const gchar *debug_track_type (CIF_Track type)
{
    switch (type) {
        case AUDIO: return "Audio";
        case MODE1: return "Mode 1";
        case MODE2_FORM1: return "Mode 2 Form 1";
        case MODE2_MIXED: return "Mode 2 Mixed";
        default: return "UNKNOWN";
    }
}


/**********************************************************************\
 *                             Helpers                                *
\**********************************************************************/
static inline gboolean gap_between_tracks (gint mode_prev, gint mode_cur)
{
    /* We create gaps on transitions between audio and data tracks */
    return (mode_prev == MIRAGE_MODE_AUDIO && mode_cur != MIRAGE_MODE_AUDIO)  || (mode_prev != MIRAGE_MODE_AUDIO && mode_cur == MIRAGE_MODE_AUDIO);
}


/**********************************************************************\
 *                   Descriptor reading and parsing                   *
\**********************************************************************/
static gboolean mirage_parser_cif_read_descriptor (MirageParserCif *self, guint8 **data, guint16 *length, GError **error)
{
    guint16 subblock_length;
    guint8 *subblock_data;

    /* Read entry length */
    if (g_input_stream_read(self->priv->cif_stream, &subblock_length, sizeof(subblock_length), NULL, NULL) != sizeof(subblock_length)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: failed to read sub-block length!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to read sub-block length!");
        return FALSE;
    }
    subblock_length = GUINT16_FROM_LE(subblock_length);

    /* Go back */
    g_seekable_seek(G_SEEKABLE(self->priv->cif_stream), -sizeof(subblock_length), G_SEEK_CUR, NULL, NULL);

    /* Sanity check */
    if (g_seekable_tell(G_SEEKABLE(self->priv->cif_stream)) + subblock_length > self->priv->disc_offset + self->priv->disc_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: sanity check failed!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Sanity check failed!");
        return FALSE;
    }

    /* Allocate buffer and read data */
    subblock_data = g_malloc(subblock_length);
    if (g_input_stream_read(self->priv->cif_stream, subblock_data, subblock_length, NULL, NULL) != subblock_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: failed to read sub-block data (%d bytes)!\n", __debug__, subblock_length);
        g_free(subblock_data);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to read sub-block data (%d bytes)!", subblock_length);
        return FALSE;
    }

    *data = subblock_data;
    *length = subblock_length;

    return TRUE;
}


static MirageTrack *mirage_parser_cif_parse_track_descriptor (MirageParserCif *self, guint8 *data, guint16 length G_GNUC_UNUSED, GError **error)
{
    MirageTrack *track;
    MirageFragment *fragment;

    CIF_TrackDescriptor *descriptor = (CIF_TrackDescriptor *)data;
    CIF_OffsetEntry *offset_entry = &self->priv->offset_entries[self->priv->track_counter++]; /* Corresponding offset entry; we also increment track counter here */

    CIF_AudioTrackDescriptor *audio_descriptor = (CIF_AudioTrackDescriptor *)(data+sizeof(CIF_TrackDescriptor));

    gint sector_size;
    gint track_length;
    gint track_mode;

    descriptor->descriptor_length = GUINT16_FROM_LE(descriptor->descriptor_length);
    descriptor->dummy1 = GUINT16_FROM_LE(descriptor->dummy1);
    descriptor->num_sectors = GUINT32_FROM_LE(descriptor->num_sectors);
    descriptor->dummy2 = GUINT16_FROM_LE(descriptor->dummy2);
    descriptor->type = GUINT16_FROM_LE(descriptor->type);
    descriptor->dummy3 = GUINT16_FROM_LE(descriptor->dummy3);
    descriptor->dummy4 = GUINT16_FROM_LE(descriptor->dummy4);
    descriptor->dummy5 = GUINT16_FROM_LE(descriptor->dummy5);
    descriptor->dao_mode = GUINT16_FROM_LE(descriptor->dao_mode);
    descriptor->dummy7 = GUINT16_FROM_LE(descriptor->dummy7);
    descriptor->sector_data_size = GUINT16_FROM_LE(descriptor->sector_data_size);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Track descriptor:\n", __debug__);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - descriptor length: %d (0x%X)\n", __debug__, descriptor->descriptor_length, descriptor->descriptor_length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy1: %d\n", __debug__, descriptor->dummy1);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - number of sectors: %d (0x%X)\n", __debug__, descriptor->num_sectors, descriptor->num_sectors);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy2: %d\n", __debug__, descriptor->dummy2);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - track type: %d (%s)\n", __debug__, descriptor->type, debug_track_type(descriptor->type));
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy3: %d\n", __debug__, descriptor->dummy3);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy4: %d\n", __debug__, descriptor->dummy4);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy5: %d\n", __debug__, descriptor->dummy5);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dao mode: %d\n", __debug__, descriptor->dao_mode);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy7: %d\n", __debug__, descriptor->dummy7);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - sector data size: %d (0x%X)\n", __debug__, descriptor->sector_data_size, descriptor->sector_data_size);

    if ((CIF_Track) descriptor->type == AUDIO) {
        audio_descriptor->fadein_length = GUINT16_FROM_LE(audio_descriptor->fadein_length);
        audio_descriptor->fadeout_length = GUINT16_FROM_LE(audio_descriptor->fadeout_length);

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Audio track descriptor:\n", __debug__);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - ISRC: %.12s\n", __debug__, audio_descriptor->isrc);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - fade-out length: %d (0x%X)\n", __debug__, audio_descriptor->fadeout_length, audio_descriptor->fadeout_length);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - fade-in length: %d (0x%X)\n", __debug__, audio_descriptor->fadein_length, audio_descriptor->fadein_length);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - title: \"%s\"\n", __debug__, audio_descriptor->title);
    }
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Offset entry:\n", __debug__);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - type: %.4s\n", __debug__, offset_entry->type);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - offset: %d (0x%X)\n", __debug__, offset_entry->offset, offset_entry->offset);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - length: %d (0x%X)\n", __debug__, offset_entry->length, offset_entry->length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");

    /* Set track mode */
    switch (descriptor->type) {
        case AUDIO: {
            track_mode = MIRAGE_MODE_AUDIO;
            sector_size = 2352;
            break;
        }
        case MODE1: {
            track_mode = MIRAGE_MODE_MODE1;
            sector_size = 2048;
            break;
        }
        case MODE2_FORM1: {
            track_mode = MIRAGE_MODE_MODE2_FORM1;
            sector_size = 2056;
            break;
        }
        case MODE2_MIXED: {
            track_mode = MIRAGE_MODE_MODE2_MIXED;
            sector_size = 2332;
            break;
        }
        default: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: unknown track type (%d)!\n", __debug__, descriptor->type);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Unknown track type (%d)!", descriptor->type);
            return NULL;
        }
    }

    /* Compute the actual track length */
    if (offset_entry->length % sector_size) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: declared data chunk length (%d) not divisible by sector size (%d)!\n", __debug__, offset_entry->length, sector_size);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Declared data chunk length (%d) not divisible by sector size (%d)!\n", offset_entry->length, sector_size);
        return NULL;
    }
    track_length = offset_entry->length / sector_size;
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: computed track length: %d (0x%X)\n", __debug__, track_length, track_length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: difference between declared and computed track length: %d (0x%X)\n", __debug__, descriptor->num_sectors - track_length, descriptor->num_sectors - track_length);


    /* Create new track */
    track = g_object_new(MIRAGE_TYPE_TRACK, NULL);

    mirage_track_set_mode(track, track_mode);

    /* Create data fragment */
    fragment = mirage_create_fragment(MIRAGE_TYPE_FRAGMENT_IFACE_BINARY, self->priv->cif_stream, G_OBJECT(self), error);
    if (!fragment) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to create fragment!\n", __debug__);
        g_object_unref(track);
        return NULL;
    }

    /* Set file */
    mirage_fragment_iface_binary_main_data_set_stream(MIRAGE_FRAGMENT_IFACE_BINARY(fragment), self->priv->cif_stream);
    mirage_fragment_iface_binary_main_data_set_offset(MIRAGE_FRAGMENT_IFACE_BINARY(fragment), offset_entry->offset);
    mirage_fragment_iface_binary_main_data_set_size(MIRAGE_FRAGMENT_IFACE_BINARY(fragment), sector_size);
    if ((CIF_Track) descriptor->type == AUDIO) {
        mirage_fragment_iface_binary_main_data_set_format(MIRAGE_FRAGMENT_IFACE_BINARY(fragment), MIRAGE_MAIN_AUDIO);
    } else {
        mirage_fragment_iface_binary_main_data_set_format(MIRAGE_FRAGMENT_IFACE_BINARY(fragment), MIRAGE_MAIN_DATA);
    }

    mirage_fragment_set_length(fragment, track_length);

    mirage_track_add_fragment(track, -1, fragment);

    g_object_unref(fragment);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");

    return track;
}

static MirageSession *mirage_parser_cif_parse_session_descriptor (MirageParserCif *self, guint8 *data, guint16 length G_GNUC_UNUSED, GError **error)
{
    MirageSession *session;
    CIF_SessionDescriptor *descriptor = (CIF_SessionDescriptor *)data;

    descriptor->descriptor_length = GUINT16_FROM_LE(descriptor->descriptor_length);
    descriptor->num_tracks = GUINT16_FROM_LE(descriptor->num_tracks);
    descriptor->dummy1 = GUINT16_FROM_LE(descriptor->dummy1);
    descriptor->dummy2 = GUINT16_FROM_LE(descriptor->dummy2);
    descriptor->dummy3 = GUINT16_FROM_LE(descriptor->dummy3);
    descriptor->session_type = GUINT16_FROM_LE(descriptor->session_type);
    descriptor->dummy4 = GUINT16_FROM_LE(descriptor->dummy4);
    descriptor->dummy5 = GUINT16_FROM_LE(descriptor->dummy5);
    descriptor->dummy6 = GUINT16_FROM_LE(descriptor->dummy6);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Session descriptor:\n", __debug__);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - descriptor length: %d (0x%X)\n", __debug__, descriptor->descriptor_length, descriptor->descriptor_length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - number of tracks: %d\n", __debug__, descriptor->num_tracks);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy1: %d\n", __debug__, descriptor->dummy1);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy2: %d\n", __debug__, descriptor->dummy2);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy3: %d\n", __debug__, descriptor->dummy3);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - session type: %d (%s)\n", __debug__, descriptor->session_type, debug_session_type(descriptor->session_type));
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy4: %d\n", __debug__, descriptor->dummy4);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy5: %d\n", __debug__, descriptor->dummy5);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy6: %d\n", __debug__, descriptor->dummy6);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");


    /* Create new session */
    session = g_object_new(MIRAGE_TYPE_SESSION, NULL);

    /* Set session type */
    switch (descriptor->session_type) {
        case CDDA: {
            mirage_session_set_session_type(session, MIRAGE_SESSION_CD_DA);
            break;
        }
        case CDROM: {
            mirage_session_set_session_type(session, MIRAGE_SESSION_CD_ROM);
            break;
        }
        case CDROMXA: {
            mirage_session_set_session_type(session, MIRAGE_SESSION_CD_ROM_XA);
            break;
        }
        default: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: unknown session type (%d)!\n", __debug__, descriptor->session_type);
            g_object_unref(session);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Unknown session type (%d)!", descriptor->session_type);
            return NULL;
        }
    }

    /* Process all tracks */
    for (gint i = 0; i < descriptor->num_tracks; i++) {
        guint8 *descriptor_data;
        guint16 descriptor_length;
        MirageTrack *track;
        gint track_mode;

        /* Read track descriptor */
        if (!mirage_parser_cif_read_descriptor(self, &descriptor_data, &descriptor_length, error)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to read track descriptor!\n", __debug__);
            g_object_unref(session);
            return NULL;
        }

        /* Parse track descriptor */
        track = mirage_parser_cif_parse_track_descriptor(self, descriptor_data, descriptor_length, error);
        if (!track) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse track descriptor!\n", __debug__);
            g_object_unref(session);
            return NULL;
        }

        track_mode = mirage_track_get_mode(track);

        /* If it is a first track in session, or if track mode has changed,
           add 150-sector pregap. */
        if (i == 0 || gap_between_tracks(self->priv->prev_track_mode, track_mode)) {
            MirageFragment *fragment;
            gint pregap_length = 150;

            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: adding pregap: %d (0x%X)\n", __debug__, pregap_length, pregap_length);

            /* Create NULL fragment - creation should never fail */
            fragment = mirage_create_fragment(MIRAGE_TYPE_FRAGMENT_IFACE_NULL, NULL, G_OBJECT(self), error);

            mirage_fragment_set_length(fragment, pregap_length);

            mirage_track_add_fragment(track, 0, fragment);
            g_object_unref(fragment);

            /* Set new track start */
            mirage_track_set_track_start(track, pregap_length);
        }

        /* Add track */
        mirage_session_add_track_by_index(session, i, track);

        g_object_unref(track);
        g_free(descriptor_data);

        self->priv->prev_track_mode = track_mode; /* Store track mode */
    }

    return session;
}


static gboolean mirage_parser_cif_parse_disc_descriptor (MirageParserCif *self, guint8 *data, guint16 length G_GNUC_UNUSED, GError **error)
{
    CIF_DiscDescriptor *descriptor = (CIF_DiscDescriptor *)data;

    descriptor->descriptor_length = GUINT16_FROM_LE(descriptor->descriptor_length);
    descriptor->num_sessions = GUINT16_FROM_LE(descriptor->num_sessions);
    descriptor->num_tracks = GUINT16_FROM_LE(descriptor->num_tracks);
    descriptor->title_length = GUINT16_FROM_LE(descriptor->title_length);
    descriptor->descriptor_length2 = GUINT16_FROM_LE(descriptor->descriptor_length2);
    descriptor->dummy1 = GUINT16_FROM_LE(descriptor->dummy1);
    descriptor->image_type = GUINT16_FROM_LE(descriptor->image_type);
    descriptor->dummy2 = GUINT16_FROM_LE(descriptor->dummy2);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Disc descriptor:\n", __debug__);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - descriptor length: %d (0x%X)\n", __debug__, descriptor->descriptor_length, descriptor->descriptor_length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - number of sessions: %d\n", __debug__, descriptor->num_sessions);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - number of tracks: %d\n", __debug__, descriptor->num_tracks);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - title length: %d (0x%X)\n", __debug__, descriptor->title_length, descriptor->title_length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - descriptor length 2: %d (0x%X)\n", __debug__, descriptor->descriptor_length2, descriptor->descriptor_length2);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy1: %d (0x%X)\n", __debug__, descriptor->dummy1, descriptor->dummy1);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - image type: %d (%s)\n", __debug__, descriptor->image_type, debug_image_type(descriptor->image_type));
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - dummy2: %d (0x%X)\n", __debug__, descriptor->dummy2, descriptor->dummy2);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - title: \"%.*s\"\n", __debug__, descriptor->title_length, descriptor->title_and_artist);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  - artist: \"%s\"\n", __debug__, descriptor->title_and_artist+descriptor->title_length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");


    /* Process all sessions */
    for (gint i = 0; i < descriptor->num_sessions; i++) {
        guint8 *descriptor_data;
        guint16 descriptor_length;
        MirageSession *session;

        /* Read session descriptor */
        if (!mirage_parser_cif_read_descriptor(self, &descriptor_data, &descriptor_length, error)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to read session descriptor!\n", __debug__);
            return FALSE;
        }

        /* Parse session descriptor */
        session = mirage_parser_cif_parse_session_descriptor(self, descriptor_data, descriptor_length, error);
        if (!session) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse session descriptor!\n", __debug__);
            return FALSE;
        }

        mirage_disc_add_session_by_index(self->priv->disc, i, session);

        g_object_unref(session);
        g_free(descriptor_data);

        /* Set leadout of the previous session */
        if (i > 0) {
            MirageSession *prev_session;
            gint leadout_length;

            prev_session = mirage_disc_get_session_by_index(self->priv->disc, i-1, error);
            if (!prev_session) {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: failed to get previous session!\n", __debug__);
                return FALSE;
            }

            /* Second session has index 1... */
            if (i == 1) {
                leadout_length = 11250; /* Actually, it should be 6750 previous leadout, 4500 current leadin */
            } else {
                leadout_length = 6750; /* Actually, it should be 2250 previous leadout, 4500 current leadin */
            }

            mirage_session_set_leadout_length(prev_session, leadout_length);

            g_object_unref(prev_session);
        }
    }


    return TRUE;
}


static gboolean mirage_parser_cif_parse_disc_block (MirageParserCif *self, GError **error)
{
    guint8 *descriptor_data;
    guint16 descriptor_length;

    /* Seek to the content of "disc" block */
    g_seekable_seek(G_SEEKABLE(self->priv->cif_stream), self->priv->disc_offset, G_SEEK_SET, NULL, NULL);

    /* Skip first 8 dummy bytes */
    g_seekable_seek(G_SEEKABLE(self->priv->cif_stream), 8, G_SEEK_CUR, NULL, NULL);

    /* Read disc descriptor */
    if (!mirage_parser_cif_read_descriptor(self, &descriptor_data, &descriptor_length, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to read disc descriptor!\n", __debug__);
        return FALSE;
    }

    /* Parse disc descriptor */
    if (!mirage_parser_cif_parse_disc_descriptor(self, descriptor_data, descriptor_length, error)) {
        g_free(descriptor_data);
        return FALSE;
    }

    g_free(descriptor_data);

    /* Sanity check */
    if (g_seekable_tell(G_SEEKABLE(self->priv->cif_stream)) != (self->priv->disc_offset + self->priv->disc_length)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: WARNING: did not finish at the end of disc block!\n", __debug__);
    }

    return TRUE;
}

static gboolean mirage_parser_cif_parse_ofs_block (MirageParserCif *self, GError **error)
{
    guint16 num_entries;

    /* Seek to the content of "ofs " block */
    g_seekable_seek(G_SEEKABLE(self->priv->cif_stream), self->priv->ofs_offset, G_SEEK_SET, NULL, NULL);

    /* Skip first 8 dummy bytes */
    g_seekable_seek(G_SEEKABLE(self->priv->cif_stream), 8, G_SEEK_CUR, NULL, NULL);

    /* Read number of entries */
    if (g_input_stream_read(self->priv->cif_stream, &num_entries, sizeof(num_entries), NULL, NULL) != sizeof(num_entries)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to read number of entries!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to read number of entries");
        return FALSE;
    }
    num_entries = GUINT16_FROM_LE(num_entries);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: number of entries: %d\n", __debug__, num_entries);

    self->priv->offset_entries = g_new0(CIF_OffsetEntry, num_entries);
    self->priv->num_offset_entries = num_entries;

    for (gint i = 0; i < num_entries && g_seekable_tell(G_SEEKABLE(self->priv->cif_stream)) < (self->priv->ofs_offset + self->priv->ofs_length); i++) {
        CIF_OffsetEntry entry;

        /* Read whole entry */
        if (g_input_stream_read(self->priv->cif_stream, &entry, sizeof(entry), NULL, NULL) != sizeof(entry)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to read off entry!\n", __debug__);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to read off entry!");
            return FALSE;
        }

        /* Match "RIFF" */
        if (memcmp(entry.riff, riff_signature, sizeof(riff_signature))) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: expected 'RIFF', got '%.4s'\n", __debug__, entry.riff);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Invalid FOURCC; expected 'RIFF', got '%.4s'", entry.riff);
            return FALSE;
        }

        entry.length = GUINT32_FROM_LE(entry.length) - 4; /* Since it includes the size of length field */
        entry.offset = GUINT32_FROM_LE(entry.offset) + 4; /* Since it's offset to type field, which we need to skip */

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: entry #%d: type '%.4s', offset %d (0x%X), length %d (0x%X)\n", __debug__, i, entry.type, entry.offset, entry.offset, entry.length, entry.length);

        /* Store */
        self->priv->offset_entries[i] = entry;
    }

    /* Sanity check */
    if (g_seekable_tell(G_SEEKABLE(self->priv->cif_stream)) != (self->priv->ofs_offset + self->priv->ofs_length)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: WARNING: did not finish at the end of ofs block!\n", __debug__);
    }

    return TRUE;
}


static gboolean mirage_parser_cif_parse_blocks (MirageParserCif *self, GError **error)
{
    guint64 file_size;
    CIF_Header header;
    guint64 offset;

    /* Get file length */
    g_seekable_seek(G_SEEKABLE(self->priv->cif_stream), 0, G_SEEK_END, NULL, NULL);
    file_size = g_seekable_tell(G_SEEKABLE(self->priv->cif_stream));

    /* Build blocks list */
    g_seekable_seek(G_SEEKABLE(self->priv->cif_stream), 0, G_SEEK_SET, NULL, NULL);
    while (g_seekable_tell(G_SEEKABLE(self->priv->cif_stream)) < file_size) {
        /* Read whole header */
        if (g_input_stream_read(self->priv->cif_stream, &header, sizeof(header), NULL, NULL) != sizeof(header)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to read header!\n", __debug__);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to read header!");
            return FALSE;
        }

        /* Match "RIFF" */
        if (memcmp(header.riff, riff_signature, sizeof(riff_signature))) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: expected 'RIFF', got '%.4s'\n", __debug__, header.riff);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Invalid FOURCC; expected 'RIFF', got '%.4s'", header.riff);
            return FALSE;
        }

        header.length = GUINT32_FROM_LE(header.length) - 4;

        /* Store data offset */
        offset = g_seekable_tell(G_SEEKABLE(self->priv->cif_stream));

        /* Skip the contents */
        g_seekable_seek(G_SEEKABLE(self->priv->cif_stream), header.length, G_SEEK_CUR, NULL, NULL);
        if (header.length % 2) {
            g_seekable_seek(G_SEEKABLE(self->priv->cif_stream), 1, G_SEEK_CUR, NULL, NULL); /* Pad byte */
        }

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: RIFF chunk of type '%.4s': offset %lld (0x%llX), length %ld (0x%lX)\n", __debug__, header.type, offset, offset, header.length, header.length);

        /* We need to store "disc" and "ofs " offsets */
        if (!memcmp(header.type, disc_signature, sizeof(disc_signature))) {
            self->priv->disc_offset = offset;
            self->priv->disc_length = header.length;
        } else if (!memcmp(header.type, ofs_signature, sizeof(ofs_signature))) {
            self->priv->ofs_offset = offset;
            self->priv->ofs_length = header.length;
        }
    }

    /* Make sure we got "disc" and "ofs " blocks */
    if (!self->priv->disc_offset || !self->priv->disc_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: 'disc' block not found!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Descriptor does not contain 'disc' block!");
        return FALSE;
    }

    if (!self->priv->ofs_offset || !self->priv->ofs_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: 'ofs ' block not found!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Descriptor does not contain 'ofs ' block!");
        return FALSE;
    }

    return TRUE;
}


static gboolean mirage_parser_cif_load_disc (MirageParserCif *self, GError **error)
{
    /* Parse blocks */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: parsing RIFF blocks\n", __debug__);
    if (!mirage_parser_cif_parse_blocks(self, error)) {
        return FALSE;
    }
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");

    /* CIF format is CD-only */
    mirage_disc_set_medium_type(self->priv->disc, MIRAGE_MEDIUM_CD);

    /* CD-ROMs start at -150 as per Red Book... */
    mirage_disc_layout_set_start_sector(self->priv->disc, -150);

    /* Parse "ofs " block */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: parsing 'ofs ' block\n", __debug__);
    if (!mirage_parser_cif_parse_ofs_block(self, error)) {
        return FALSE;
    }
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");

    /* Parse "disc" block */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: parsing 'disc' block\n", __debug__);
    if (!mirage_parser_cif_parse_disc_block(self, error)) {
        return FALSE;
    }
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");

    return TRUE;
}


/**********************************************************************\
 *                MirageParser methods implementation                *
\**********************************************************************/
static MirageDisc *mirage_parser_cif_load_image (MirageParser *_self, GInputStream **streams, GError **error)
{
    MirageParserCif *self = MIRAGE_PARSER_CIF(_self);
    const gchar *cif_filename;
    gboolean succeeded = TRUE;
    CIF_Header header;

    /* Check file signature */
    self->priv->cif_stream = streams[0];
    g_object_ref(self->priv->cif_stream);

    if (g_input_stream_read(self->priv->cif_stream, &header, sizeof(header), NULL, NULL) != sizeof(header)) {
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to read header!");
        return FALSE;
    }

    /* Match "RIFF" at the beginning and "imag" that comes after length
       field. We could probably also match length, since it appears to
       be fixed, but this should sufficient... */
    if (memcmp(header.riff, riff_signature, sizeof(riff_signature)) || memcmp(header.type, imag_signature, sizeof(imag_signature))) {
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_CANNOT_HANDLE, "Parser cannot handle given image!");
        return FALSE;
    }

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: parsing the image...\n", __debug__);

    /* Create disc */
    self->priv->disc = g_object_new(MIRAGE_TYPE_DISC, NULL);
    mirage_object_attach_child(MIRAGE_OBJECT(self), self->priv->disc);

    cif_filename = mirage_get_file_stream_filename(self->priv->cif_stream);
    mirage_disc_set_filename(self->priv->disc, cif_filename);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: CIF filename: %s\n", __debug__, cif_filename);

    /* Load disc */
    succeeded = mirage_parser_cif_load_disc(self, error);

    /* Return disc */
    mirage_object_detach_child(MIRAGE_OBJECT(self), self->priv->disc);
    if (succeeded) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: parsing completed successfully\n\n", __debug__);
        return self->priv->disc;
    } else {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: parsing failed!\n\n", __debug__);
        g_object_unref(self->priv->disc);
        return NULL;
    }
}


/**********************************************************************\
 *                             Object init                            *
\**********************************************************************/
G_DEFINE_DYNAMIC_TYPE(MirageParserCif, mirage_parser_cif, MIRAGE_TYPE_PARSER);

void mirage_parser_cif_type_register (GTypeModule *type_module)
{
    return mirage_parser_cif_register_type(type_module);
}


static void mirage_parser_cif_init (MirageParserCif *self)
{
    self->priv = MIRAGE_PARSER_CIF_GET_PRIVATE(self);

    mirage_parser_generate_info(MIRAGE_PARSER(self),
        "PARSER-CIF",
        "CIF Image Parser",
        "CIF (Adaptec Easy CD Creator) images",
        "application/x-cif"
    );

    self->priv->offset_entries = NULL;

    self->priv->track_counter = 0;
}

static void mirage_parser_cif_dispose (GObject *gobject)
{
    MirageParserCif *self = MIRAGE_PARSER_CIF(gobject);

    if (self->priv->cif_stream) {
        g_object_unref(self->priv->cif_stream);
        self->priv->cif_stream = NULL;
    }

    /* Chain up to the parent class */
    return G_OBJECT_CLASS(mirage_parser_cif_parent_class)->dispose(gobject);
}

static void mirage_parser_cif_finalize (GObject *gobject)
{
    MirageParserCif *self = MIRAGE_PARSER_CIF(gobject);

    g_free(self->priv->offset_entries);

    /* Chain up to the parent class */
    return G_OBJECT_CLASS(mirage_parser_cif_parent_class)->finalize(gobject);
}

static void mirage_parser_cif_class_init (MirageParserCifClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    MirageParserClass *parser_class = MIRAGE_PARSER_CLASS(klass);

    gobject_class->dispose = mirage_parser_cif_dispose;
    gobject_class->finalize = mirage_parser_cif_finalize;

    parser_class->load_image = mirage_parser_cif_load_image;

    /* Register private structure */
    g_type_class_add_private(klass, sizeof(MirageParserCifPrivate));
}

static void mirage_parser_cif_class_finalize (MirageParserCifClass *klass G_GNUC_UNUSED)
{
}
