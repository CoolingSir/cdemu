/*
 *  libMirage: BINARY fragment: Fragment object
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

#include "fragment-binary.h"

#define __debug__ "BINARY-Fragment"


/**********************************************************************\
 *                          Private structure                         *
\**********************************************************************/
#define MIRAGE_FRAGMENT_BINARY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), MIRAGE_TYPE_FRAGMENT_BINARY, MIRAGE_Fragment_BINARYPrivate))

struct _MIRAGE_Fragment_BINARYPrivate
{
    gchar *tfile_name; /* Track file name */
    GObject *tfile_stream; /* Track file stream */
    gint tfile_sectsize; /* Track file sector size */
    gint tfile_format; /* Track file format */
    guint64 tfile_offset; /* Track offset in track file */

    gchar *sfile_name; /* Subchannel file name */
    GObject *sfile_stream; /* Subchannel file stream */
    gint sfile_sectsize; /* Subchannel file sector size*/
    gint sfile_format; /* Subchannel file format */
    guint64 sfile_offset; /* Subchannel offset in subchannel file */
};


/**********************************************************************\
 *                     Binary Interface implementation                *
\**********************************************************************/
static gboolean mirage_fragment_binary_track_file_set_file (MIRAGE_FragIface_Binary *_self, const gchar *filename, GObject *stream, GError **error)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    GError *local_error = NULL;

    /* Release old stream */
    if (self->priv->tfile_stream) {
        g_object_unref(self->priv->tfile_stream);
        self->priv->tfile_stream = NULL;
    }
    if (self->priv->tfile_name) {
        g_free(self->priv->tfile_name);
        self->priv->tfile_name = NULL;
    }

    /* Set new stream */
    if (stream) {
        /* Set the provided stream */
        self->priv->tfile_stream = stream;
        g_object_ref(stream);
    } else {
        /* Open new stream */
        self->priv->tfile_stream = libmirage_create_file_stream(filename, G_OBJECT(self), &local_error);
        if (!self->priv->tfile_stream) {
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_FRAGMENT_ERROR, "Failed to create file stream on track data file '%s': %s", filename, local_error->message);
            g_error_free(local_error);
            return FALSE;
        }
    }
    self->priv->tfile_name = g_strdup(filename);

    return TRUE;
}

static const gchar *mirage_fragment_binary_track_file_get_file (MIRAGE_FragIface_Binary *_self)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Return file name */
    return self->priv->tfile_name;
}


static void mirage_fragment_binary_track_file_set_offset (MIRAGE_FragIface_Binary *_self, guint64 offset)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Set offset */
    self->priv->tfile_offset = offset;
}

static guint64 mirage_fragment_binary_track_file_get_offset (MIRAGE_FragIface_Binary *_self)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Return offset */
    return self->priv->tfile_offset;
}


static void mirage_fragment_binary_track_file_set_sectsize (MIRAGE_FragIface_Binary *_self, gint sectsize)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Set sector size */
    self->priv->tfile_sectsize = sectsize;
}

static gint mirage_fragment_binary_track_file_get_sectsize (MIRAGE_FragIface_Binary *_self)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Return sector size */
    return self->priv->tfile_sectsize;
}


static void mirage_fragment_binary_track_file_set_format (MIRAGE_FragIface_Binary *_self, gint format)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Set format */
    self->priv->tfile_format = format;
}

static gint mirage_fragment_binary_track_file_get_format (MIRAGE_FragIface_Binary *_self)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Return format */
    return self->priv->tfile_format;
}


static guint64 mirage_fragment_binary_track_file_get_position (MIRAGE_FragIface_Binary *_self, gint address)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    gint sectsize_full;

    /* Calculate 'full' sector size:
        -> track data + subchannel data, if there's internal subchannel
        -> track data, if there's external or no subchannel
    */

    sectsize_full = self->priv->tfile_sectsize;
    if (self->priv->sfile_format & FR_BIN_SFILE_INT) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: internal subchannel, adding %d to sector size %d\n", __debug__, self->priv->sfile_sectsize, sectsize_full);
        sectsize_full += self->priv->sfile_sectsize;
    }

    /* We assume address is relative address */
    /* guint64 casts are required so that the product us 64-bit; product of two
       32-bit integers would be 32-bit, which would be truncated at overflow... */
    return self->priv->tfile_offset + (guint64)address * (guint64)sectsize_full;
}


static gboolean mirage_fragment_binary_subchannel_file_set_file (MIRAGE_FragIface_Binary *_self, const gchar *filename, GObject *stream, GError **error)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    GError *local_error = NULL;

    /* Release old stream */
    if (self->priv->sfile_stream) {
        g_object_unref(self->priv->sfile_stream);
        self->priv->sfile_stream = NULL;
    }
    if (self->priv->sfile_name) {
        g_free(self->priv->sfile_name);
        self->priv->sfile_name = NULL;
    }

    /* Set new stream */
    if (stream) {
        /* Set the provided stream */
        self->priv->sfile_stream = stream;
        g_object_ref(stream);
    } else {
        /* Try opening a stream */
        self->priv->sfile_stream = libmirage_create_file_stream(filename, G_OBJECT(self), &local_error);
        if (!self->priv->sfile_stream) {
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_FRAGMENT_ERROR, "Failed to create file stream on subchannel data file '%s': %s", filename, local_error->message);
            g_error_free(local_error);
            return FALSE;
        }
    }
    self->priv->sfile_name = g_strdup(filename);

    return TRUE;
}

static const gchar *mirage_fragment_binary_subchannel_file_get_file (MIRAGE_FragIface_Binary *_self)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Return file name */
    return self->priv->sfile_name;
}


static void mirage_fragment_binary_subchannel_file_set_offset (MIRAGE_FragIface_Binary *_self, guint64 offset)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Set offset */
    self->priv->sfile_offset = offset;
}

static guint64 mirage_fragment_binary_subchannel_file_get_offset (MIRAGE_FragIface_Binary *_self)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Return offset */
    return self->priv->sfile_offset;
}

static void mirage_fragment_binary_subchannel_file_set_sectsize (MIRAGE_FragIface_Binary *_self, gint sectsize)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Set sector size */
    self->priv->sfile_sectsize = sectsize;
}

static gint mirage_fragment_binary_subchannel_file_get_sectsize (MIRAGE_FragIface_Binary *_self)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Return sector size */
    return self->priv->sfile_sectsize;
}


static void mirage_fragment_binary_subchannel_file_set_format (MIRAGE_FragIface_Binary *_self, gint format)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Set format */
    self->priv->sfile_format = format;
}

static gint mirage_fragment_binary_subchannel_file_get_format (MIRAGE_FragIface_Binary *_self)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    /* Return format */
    return self->priv->sfile_format;
}


static guint64 mirage_fragment_binary_subchannel_file_get_position (MIRAGE_FragIface_Binary *_self, gint address)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    guint64 offset = 0;

    /* Either we have internal or external subchannel */
    if (self->priv->sfile_format & FR_BIN_SFILE_INT) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: internal subchannel, position is at end of main channel data\n", __debug__);
        /* Subchannel is contained in track file; get position in track file
           for that sector, and add to it length of track data sector */
        offset = mirage_frag_iface_binary_track_file_get_position(MIRAGE_FRAG_IFACE_BINARY(self), address);
        offset += self->priv->tfile_sectsize;
    } else if (self->priv->sfile_format & FR_BIN_SFILE_EXT) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: external subchannel, calculating position\n", __debug__);
        /* We assume address is relative address */
        /* guint64 casts are required so that the product us 64-bit; product of two
           32-bit integers would be 32-bit, which would be truncated at overflow... */
        offset = self->priv->sfile_offset + (guint64)address * (guint64)self->priv->sfile_sectsize;
    }

    return offset;
}


/**********************************************************************\
 *               MIRAGE_Fragment methods implementations              *
\**********************************************************************/
static gboolean mirage_fragment_binary_can_handle_data_format (MIRAGE_Fragment *_self G_GNUC_UNUSED, GObject *stream, GError **error)
{
    /* Make sure stream is provided */
    if (!stream) {
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_CANNOT_HANDLE, "Fragment cannot handle given data!");
        return FALSE;
    }

    /* BINARY doesn't need any other data checks; what's important is interface type,
       which is filtered out elsewhere */
    return TRUE;
}

static gboolean mirage_fragment_binary_use_the_rest_of_file (MIRAGE_Fragment *_self, GError **error)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);
    GError *local_error = NULL;

    goffset file_size;
    gint full_sectsize;
    gint fragment_len;

    if (!self->priv->tfile_stream) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: data file stream not set!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_FRAGMENT_ERROR, "Track data file stream not set!");
        return FALSE;
    }

    /* Get file length */
    if (!g_seekable_seek(G_SEEKABLE(self->priv->tfile_stream), 0, G_SEEK_END, NULL, &local_error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to seek to the end of track data file stream: %s\n", __debug__, local_error->message);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_FRAGMENT_ERROR, "Failed to seek to the end of track data file stream: %s", local_error->message);
        g_error_free(local_error);
        return FALSE;
    }
    file_size = g_seekable_tell(G_SEEKABLE(self->priv->tfile_stream));

    /* Use all data from offset on... */
    full_sectsize = self->priv->tfile_sectsize;
    if (self->priv->sfile_format & FR_BIN_SFILE_INT) {
        full_sectsize += self->priv->sfile_sectsize;
    }

    fragment_len = (file_size - self->priv->tfile_offset) / full_sectsize;
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: using the rest of file (%d sectors)\n", __debug__, fragment_len);

    /* Set the length */
    mirage_fragment_set_length(MIRAGE_FRAGMENT(self), fragment_len);
    return TRUE;
}

static gboolean mirage_fragment_binary_read_main_data (MIRAGE_Fragment *_self, gint address, guint8 *buf, gint *length, GError **error G_GNUC_UNUSED)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);

    guint64 position;
    gint read_len;

    /* We need file to read data from... but if it's missing, we don't read
       anything and this is not considered an error */
    if (!self->priv->tfile_stream) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: no data file stream!\n", __debug__);
        if (length) {
            *length = 0;
        }
        return TRUE;
    }

    /* Determine position within file */
    position = mirage_frag_iface_binary_track_file_get_position(MIRAGE_FRAG_IFACE_BINARY(self), address);

    if (buf) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: reading from position 0x%llX\n", __debug__, position);

        /* Note: we ignore all errors here in order to be able to cope with truncated mini images */
        g_seekable_seek(G_SEEKABLE(self->priv->tfile_stream), position, G_SEEK_SET, NULL, NULL);
        read_len = g_input_stream_read(G_INPUT_STREAM(self->priv->tfile_stream), buf, self->priv->tfile_sectsize, NULL, NULL);

        /*if (read_len != self->priv->tfile_sectsize) {
            mirage_error(MIRAGE_E_READFAILED, error);
            return FALSE;
        }*/

        /* Binary audio files may need to be swapped from BE to LE */
        if (self->priv->tfile_format == FR_BIN_TFILE_AUDIO_SWAP) {
            gint i;
            for (i = 0; i < read_len; i+=2) {
                guint16 *ptr = (guint16 *)&buf[i];
                *ptr = GUINT16_SWAP_LE_BE(*ptr);
            }
        }
    }

    if (length) {
        *length = self->priv->tfile_sectsize;
    }

    return TRUE;
}

static gboolean mirage_fragment_binary_read_subchannel_data (MIRAGE_Fragment *_self, gint address, guint8 *buf, gint *length, GError **error G_GNUC_UNUSED)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(_self);

    GObject *stream;
    guint64 position;
    gint read_len;


    /* If there's no subchannel, return 0 for the length */
    if (!self->priv->sfile_sectsize) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: no subchannel (sectsize = 0)!\n", __debug__);
        if (length) {
            *length = 0;
        }
        return TRUE;
    }

    /* We need file to read data from... but if it's missing, we don't read
       anything and this is not considered an error */
    if (self->priv->sfile_format & FR_BIN_SFILE_INT) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: internal subchannel, using track file handle\n", __debug__);
        stream = self->priv->tfile_stream;
    } else {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: external subchannel, using track file handle\n", __debug__);
        stream = self->priv->sfile_stream;
    }

    if (!stream) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: no stream!\n", __debug__);
        if (length) {
            *length = 0;
        }
        return TRUE;
    }


    /* Determine position within file */
    position = mirage_frag_iface_binary_subchannel_file_get_position(MIRAGE_FRAG_IFACE_BINARY(self), address);

    /* Read only if there's buffer to read into */
    if (buf) {
        guint8 tmp_buf[96];

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_FRAGMENT, "%s: reading from position 0x%llX\n", __debug__, position);
        /* We read into temporary buffer, because we might need to perform some
           magic on the data */
        g_seekable_seek(G_SEEKABLE(stream), position, G_SEEK_SET, NULL, NULL);
        read_len = g_input_stream_read(G_INPUT_STREAM(stream), tmp_buf, self->priv->sfile_sectsize, NULL, NULL);

        if (read_len != self->priv->sfile_sectsize) {
            /*mirage_error(MIRAGE_E_READFAILED, error);
            return FALSE;*/
        }

        /* If we happen to deal with anything that's not RAW 96-byte interleaved PW,
           we transform it into that here... less fuss for upper level stuff this way */
        if (self->priv->sfile_format & FR_BIN_SFILE_PW96_LIN) {
            /* 96-byte deinterleaved PW; grab each subchannel and interleave it
               into destination buffer */
            gint i;

            for (i = 0; i < 8; i++) {
                guint8 *ptr = tmp_buf + i*12;
                mirage_helper_subchannel_interleave(7 - i, ptr, buf);
            }
        } else if (self->priv->sfile_format & FR_BIN_SFILE_PW96_INT) {
            /* 96-byte interleaved PW; just copy it */
            memcpy(buf, tmp_buf, 96);
        } else if (self->priv->sfile_format & FR_BIN_SFILE_PQ16) {
            /* 16-byte PQ; interleave it and pretend everything else's 0 */
            mirage_helper_subchannel_interleave(SUBCHANNEL_Q, tmp_buf, buf);
        }
    }

    if (length) {
        *length = 96; /* Always 96, because we do the processing here */
    }

    return TRUE;
}


/**********************************************************************\
 *                             Object init                            *
\**********************************************************************/
static void mirage_fragment_binary_frag_iface_binary_init (MIRAGE_FragIface_BinaryInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(MIRAGE_Fragment_BINARY,
                               mirage_fragment_binary,
                               MIRAGE_TYPE_FRAGMENT,
                               0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(MIRAGE_TYPE_FRAG_IFACE_BINARY,
                                                             mirage_fragment_binary_frag_iface_binary_init));

void mirage_fragment_binary_type_register (GTypeModule *type_module)
{
    return mirage_fragment_binary_register_type(type_module);
}


static void mirage_fragment_binary_init (MIRAGE_Fragment_BINARY *self)
{
    self->priv = MIRAGE_FRAGMENT_BINARY_GET_PRIVATE(self);

    mirage_fragment_generate_fragment_info(MIRAGE_FRAGMENT(self),
        "FRAGMENT-BINARY",
        "Binary Fragment"
    );

    self->priv->tfile_name = NULL;
    self->priv->tfile_stream = NULL;
    self->priv->sfile_name = NULL;
    self->priv->sfile_stream = NULL;
}

static void mirage_fragment_binary_dispose (GObject *gobject)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(gobject);

    if (self->priv->tfile_stream) {
        g_object_unref(self->priv->tfile_stream);
        self->priv->tfile_stream = NULL;
    }
    if (self->priv->sfile_stream) {
        g_object_unref(self->priv->sfile_stream);
        self->priv->sfile_stream = NULL;
    }

    /* Chain up to the parent class */
    return G_OBJECT_CLASS(mirage_fragment_binary_parent_class)->dispose(gobject);
}

static void mirage_fragment_binary_finalize (GObject *gobject)
{
    MIRAGE_Fragment_BINARY *self = MIRAGE_FRAGMENT_BINARY(gobject);

    g_free(self->priv->tfile_name);
    g_free(self->priv->sfile_name);

    /* Chain up to the parent class */
    return G_OBJECT_CLASS(mirage_fragment_binary_parent_class)->finalize(gobject);
}

static void mirage_fragment_binary_class_init (MIRAGE_Fragment_BINARYClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    MIRAGE_FragmentClass *fragment_class = MIRAGE_FRAGMENT_CLASS(klass);

    gobject_class->dispose = mirage_fragment_binary_dispose;
    gobject_class->finalize = mirage_fragment_binary_finalize;

    fragment_class->can_handle_data_format = mirage_fragment_binary_can_handle_data_format;
    fragment_class->use_the_rest_of_file = mirage_fragment_binary_use_the_rest_of_file;
    fragment_class->read_main_data = mirage_fragment_binary_read_main_data;
    fragment_class->read_subchannel_data = mirage_fragment_binary_read_subchannel_data;

    /* Register private structure */
    g_type_class_add_private(klass, sizeof(MIRAGE_Fragment_BINARYPrivate));
}

static void mirage_fragment_binary_class_finalize (MIRAGE_Fragment_BINARYClass *klass G_GNUC_UNUSED)
{
}


static void mirage_fragment_binary_frag_iface_binary_init (MIRAGE_FragIface_BinaryInterface *iface)
{
    iface->track_file_set_file = mirage_fragment_binary_track_file_set_file;
    iface->track_file_get_file = mirage_fragment_binary_track_file_get_file;
    iface->track_file_set_offset = mirage_fragment_binary_track_file_set_offset;
    iface->track_file_get_offset = mirage_fragment_binary_track_file_get_offset;
    iface->track_file_set_sectsize = mirage_fragment_binary_track_file_set_sectsize;
    iface->track_file_get_sectsize = mirage_fragment_binary_track_file_get_sectsize;
    iface->track_file_set_format = mirage_fragment_binary_track_file_set_format;
    iface->track_file_get_format = mirage_fragment_binary_track_file_get_format;

    iface->track_file_get_position = mirage_fragment_binary_track_file_get_position;

    iface->subchannel_file_set_file = mirage_fragment_binary_subchannel_file_set_file;
    iface->subchannel_file_get_file = mirage_fragment_binary_subchannel_file_get_file;
    iface->subchannel_file_set_offset = mirage_fragment_binary_subchannel_file_set_offset;
    iface->subchannel_file_get_offset = mirage_fragment_binary_subchannel_file_get_offset;
    iface->subchannel_file_set_sectsize = mirage_fragment_binary_subchannel_file_set_sectsize;
    iface->subchannel_file_get_sectsize = mirage_fragment_binary_subchannel_file_get_sectsize;
    iface->subchannel_file_set_format = mirage_fragment_binary_subchannel_file_set_format;
    iface->subchannel_file_get_format = mirage_fragment_binary_subchannel_file_get_format;

    iface->subchannel_file_get_position = mirage_fragment_binary_subchannel_file_get_position;
}
