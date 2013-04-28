/*
 *  libMirage: Hard-disk image parser: Parser object
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

#include "image-harddisk.h"

#define __debug__ "HD-Parser"


/**********************************************************************\
 *                          Private structure                         *
\**********************************************************************/
#define MIRAGE_PARSER_HD_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), MIRAGE_TYPE_PARSER_HD, MirageParserHdPrivate))

struct _MirageParserHdPrivate
{
    MirageDisc *disc;

    gint track_mode;
    gint track_sectsize;

    gboolean needs_padding;
};


static gboolean mirage_parser_hd_is_file_valid (MirageParserHd *self, GInputStream *stream, GError **error)
{
    gsize file_length;

    /* Get stream length */
    if (!g_seekable_seek(G_SEEKABLE(stream), 0, G_SEEK_END, NULL, error)) {
        return FALSE;
    }
    file_length = g_seekable_tell(G_SEEKABLE(stream));

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: verifying file size...\n", __debug__);

    /* Make sure the file is large enough to contain a DDM */
    if (file_length < 512) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: parser cannot handle given image: file too small!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_CANNOT_HANDLE, "Parser cannot handle given image: file too small!");
        return FALSE;
    }

    /* Macintosh .CDR image check */
    /* These are raw images starting with a 512-byte Driver Descriptor Map (DDM) with an "ER" signature 
       with one of these following:
       1) 512-byte Apple (APM) Partition Map entries with a "PM signature".
       2) 512-byte GUID (GPT) Partition Table Header with a "EFI PART" signature, and 128-byte Partition Map Entries.
       3) Unpartitioned device.
     */
    guint8  mac_buf[512];
    guint16 mac_sectsize;
    guint32 mac_dev_sectors;

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: checking if image is a Macintosh DVD/CD Master (CDR) image...\n", __debug__);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: looking for 'ER' signature at the beginning of file\n", __debug__);

    if (!g_seekable_seek(G_SEEKABLE(stream), 0, G_SEEK_SET, NULL, NULL)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to seek to beginning of file!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to seek to beginning of file!");
        return FALSE;
    }

    if (g_input_stream_read(stream, mac_buf, sizeof(mac_buf), NULL, NULL) != sizeof(mac_buf)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to read first 512 bytes of file!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to read first 512 bytes of file!");
        return FALSE;
    }

    if (!memcmp(mac_buf, "ER", 2)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: 'ER' signature found!\n", __debug__);

        mac_sectsize    = GUINT16_FROM_BE(*((guint16 *) mac_buf + 1));
        mac_dev_sectors = GUINT32_FROM_BE(*((guint32 *) mac_buf + 1));

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: image is a CDR image; %d sectors, sector size: %d\n", __debug__, mac_dev_sectors, mac_sectsize);

        if (mac_sectsize == 512 || mac_sectsize == 1024) {
            self->priv->needs_padding = file_length % 2048;
        } else if (mac_sectsize == 2048) {
            self->priv->needs_padding = FALSE;
        } else {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: parser cannot map this sector size!\n", __debug__);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Parser cannot map this sector size!");
            return FALSE;
        }
        self->priv->track_sectsize = 2048;
        self->priv->track_mode = MIRAGE_MODE_MODE1;

        return TRUE;
    } else {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: 'ER' signature not found!\n", __debug__);
    }

    /* Nope, can't load the file */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: parser cannot handle given image!\n", __debug__);
    g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_CANNOT_HANDLE, "Parser cannot handle given image!");
    return FALSE;
}

static gboolean mirage_parser_hd_load_track (MirageParserHd *self, GInputStream *stream, GError **error)
{
    MirageSession *session;
    MirageTrack *track;
    MirageFragment *fragment;

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: loading track...\n", __debug__);
    
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: track mode: %d\n", __debug__, self->priv->track_mode);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: sector size: %d\n", __debug__, self->priv->track_sectsize);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: padding sector needed: %d\n", __debug__, self->priv->needs_padding);

    /* Create data fragment */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: creating data fragment\n", __debug__);
    fragment = g_object_new(MIRAGE_TYPE_FRAGMENT, NULL);

    mirage_fragment_main_data_set_stream(fragment, stream);
    mirage_fragment_main_data_set_size(fragment, self->priv->track_sectsize);
    mirage_fragment_main_data_set_format(fragment, MIRAGE_MAIN_DATA);

    /* Use whole file */
    if (!mirage_fragment_use_the_rest_of_file(fragment, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to use the rest of file!\n", __debug__);
        g_object_unref(fragment);
        return FALSE;
    }

    /* Add one sector to cover otherwise truncated data */
    if (self->priv->needs_padding) {
        gint cur_length = mirage_fragment_get_length(fragment);
        mirage_fragment_set_length(fragment, cur_length + 1);
    }

    /* Add track */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: adding track\n", __debug__);

    session = mirage_disc_get_session_by_index(self->priv->disc, -1, NULL);

    track = g_object_new(MIRAGE_TYPE_TRACK, NULL);
    mirage_session_add_track_by_index(session, -1, track);
    g_object_unref(session);

    /* Set track mode */
    mirage_track_set_mode(track, self->priv->track_mode);

    /* Add fragment to track */
    mirage_track_add_fragment(track, -1, fragment);

    g_object_unref(fragment);
    g_object_unref(track);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: finished loading track\n", __debug__);

    return TRUE;
}


/**********************************************************************\
 *                MirageParser methods implementation                *
\**********************************************************************/
static MirageDisc *mirage_parser_hd_load_image (MirageParser *_self, GInputStream **streams, GError **error)
{
    MirageParserHd *self = MIRAGE_PARSER_HD(_self);
    const gchar *hd_filename;
    GInputStream *stream;
    gboolean succeeded = TRUE;

    /* Check if file can be loaded */
    stream = streams[0];
    g_object_ref(stream);
    hd_filename = mirage_contextual_get_file_stream_filename(MIRAGE_CONTEXTUAL(self), stream);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: checking if parser can handle given image...\n", __debug__);
    if (!mirage_parser_hd_is_file_valid(self, stream, error)) {
        g_object_unref(stream);
        return FALSE;
    }
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: parser can handle given image!\n", __debug__);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: parsing the image...\n", __debug__);

    /* Create disc */
    self->priv->disc = g_object_new(MIRAGE_TYPE_DISC, NULL);
    mirage_object_set_parent(MIRAGE_OBJECT(self->priv->disc), self);

    /* Set filenames */
    mirage_disc_set_filename(self->priv->disc, hd_filename);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Hard-disk filename: %s\n", __debug__, hd_filename);

    /* Session: one session (with one tracks) */
    MirageSession *session = g_object_new(MIRAGE_TYPE_SESSION, NULL);
    mirage_disc_add_session_by_index(self->priv->disc, 0, session);

    /* Hard-disk image parser assumes single-track image, so we're dealing with regular CD-ROM session */
    mirage_session_set_session_type(session, MIRAGE_SESSION_CD_ROM);
    g_object_unref(session);

    /* Load track */
    if (!mirage_parser_hd_load_track(self, stream, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to load track!\n", __debug__);
        succeeded = FALSE;
        goto end;
    }

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: finishing the layout\n", __debug__);

    /* Now guess medium type and if it's a CD-ROM, add Red Book pregap */
    gint medium_type = mirage_parser_guess_medium_type(MIRAGE_PARSER(self), self->priv->disc);
    mirage_disc_set_medium_type(self->priv->disc, medium_type);
    if (medium_type == MIRAGE_MEDIUM_CD) {
        mirage_parser_add_redbook_pregap(MIRAGE_PARSER(self), self->priv->disc);
    }

end:
    g_object_unref(stream);

    /* Return disc */
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
G_DEFINE_DYNAMIC_TYPE(MirageParserHd, mirage_parser_hd, MIRAGE_TYPE_PARSER);

void mirage_parser_hd_type_register (GTypeModule *type_module)
{
    return mirage_parser_hd_register_type(type_module);
}


static void mirage_parser_hd_init (MirageParserHd *self)
{
    self->priv = MIRAGE_PARSER_HD_GET_PRIVATE(self);

    mirage_parser_generate_info(MIRAGE_PARSER(self),
        "PARSER-HD",
        "Hard-disk Image Parser",
        1,
        "Macintosh DVD/CD Master images (*.cdr)", "application/x-apple-cdr"
    );

    self->priv->needs_padding = FALSE;
}

static void mirage_parser_hd_class_init (MirageParserHdClass *klass)
{
    MirageParserClass *parser_class = MIRAGE_PARSER_CLASS(klass);

    parser_class->load_image = mirage_parser_hd_load_image;

    /* Register private structure */
    g_type_class_add_private(klass, sizeof(MirageParserHdPrivate));
}

static void mirage_parser_hd_class_finalize (MirageParserHdClass *klass G_GNUC_UNUSED)
{
}
