/*
 *  libMirage: File stream object
 *  Copyright (C) 2014 Rok Mandeljc
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

/**
 * SECTION: mirage-file-stream
 * @title: MirageFileStream
 * @short_description: File stream object.
 * @see_also: #MirageStream, #MirageFilterStream
 * @include: mirage-file-stream.h
 *
 * TODO
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mirage.h"

#define __debug__ "FileStream"


/**********************************************************************\
 *                          Private structure                         *
\**********************************************************************/
#define MIRAGE_FILE_STREAM_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), MIRAGE_TYPE_FILE_STREAM, MirageFileStreamPrivate))

struct _MirageFileStreamPrivate
{
    /* These two are just convenience pointers */
    GInputStream *input_stream;
    GOutputStream *output_stream;

    /* Actual stream object; either GFileIOStream or GFileInputStream,
       depending on whether stream is read-write or read-only */
    gpointer stream;

    /* Filename the stream was opened on */
    gchar *filename;
};


/**********************************************************************\
 *                             Public API                             *
\**********************************************************************/
gboolean mirage_file_stream_open (MirageFileStream *self, const gchar *filename, gboolean writable, GError **error)
{
    GFile *file;
    GFileType file_type;
    GError *local_error = NULL;

    /* Clear old stream */
    if (self->priv->stream) {
        g_object_unref(self->priv->stream);
        self->priv->stream = NULL;
    }

    g_free(self->priv->filename);
    self->priv->filename = NULL;

    self->priv->input_stream = NULL;
    self->priv->output_stream = NULL;

    /* Open file; at the bottom of the chain, there's always a GFileStream */
    file = g_file_new_for_path(filename);

    if (writable) {
        /* Create GFileIOStream */
        self->priv->stream = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_PRIVATE | G_FILE_CREATE_REPLACE_DESTINATION, NULL, &local_error);

        if (self->priv->stream) {
            self->priv->input_stream = g_io_stream_get_input_stream(G_IO_STREAM(self->priv->stream));
            self->priv->output_stream = g_io_stream_get_output_stream(G_IO_STREAM(self->priv->stream));
        }
    } else {
        /* If opening in readonly mode, file must exist */
        file_type = g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, NULL);
        if (!(file_type == G_FILE_TYPE_REGULAR || file_type == G_FILE_TYPE_SYMBOLIC_LINK || file_type == G_FILE_TYPE_SHORTCUT)) {
            g_object_unref(file);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_STREAM_ERROR, "File '%s' does not exist!", filename);
            return FALSE;
        }

        /* Create GFileInputStream */
        self->priv->stream = g_file_read(file, NULL, &local_error);

        if (self->priv->stream) {
            self->priv->input_stream = G_INPUT_STREAM(self->priv->stream);
        }
    }

    g_object_unref(file);

    if (!self->priv->stream) {
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_STREAM_ERROR, "Failed to open file %s stream on data file '%s': %s!", writable ? "input/output" : "input", filename, local_error->message);
        g_error_free(local_error);
        return FALSE;
    }

    /* Store filename */
    self->priv->filename = g_strdup(filename);

    return TRUE;
}


/**********************************************************************\
 *                MirageStream methods implementations                *
\**********************************************************************/
static const gchar *mirage_file_stream_get_filename (MirageStream *_self)
{
    MirageFileStream *self = MIRAGE_FILE_STREAM(_self);
    return self->priv->filename;
}

static gboolean mirage_file_stream_is_writable (MirageStream *_self)
{
    MirageFileStream *self = MIRAGE_FILE_STREAM(_self);
    return self->priv->output_stream != NULL;
}


static gssize mirage_file_stream_read (MirageStream *_self, void *buffer, gsize count, GError **error)
{
    MirageFileStream *self = MIRAGE_FILE_STREAM(_self);

    if (!self->priv->input_stream) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: no file input stream!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_STREAM_ERROR, "No file input stream!");
        return -1;
    }

    return g_input_stream_read(self->priv->input_stream, buffer, count, NULL, error);
}

static gssize mirage_file_stream_write (MirageStream *_self, const void *buffer, gsize count, GError **error)
{
    MirageFileStream *self = MIRAGE_FILE_STREAM(_self);

    if (!self->priv->output_stream) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: no file output stream!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_STREAM_ERROR, "No file output stream!");
        return -1;
    }

    return g_output_stream_write(self->priv->output_stream, buffer, count, NULL, error);
}

static gboolean mirage_file_stream_seek (MirageStream *_self, goffset offset, GSeekType type, GError **error)
{
    MirageFileStream *self = MIRAGE_FILE_STREAM(_self);

    if (!self->priv->stream) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: no file stream!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_STREAM_ERROR, "No file stream!");
        return FALSE;
    }

    return g_seekable_seek(G_SEEKABLE(self->priv->stream), offset, type, NULL, error);
}


static goffset mirage_file_stream_tell (MirageStream *_self)
{
    MirageFileStream *self = MIRAGE_FILE_STREAM(_self);

    if (!self->priv->stream) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: no file stream!\n", __debug__);
        return -1;
    }

    return g_seekable_tell(G_SEEKABLE(self->priv->stream));
}


/**********************************************************************\
 *                             Object init                            *
\**********************************************************************/
static void mirage_file_stream_stream_init (MirageStreamInterface *iface);

G_DEFINE_TYPE_WITH_CODE(MirageFileStream, mirage_file_stream, MIRAGE_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(MIRAGE_TYPE_STREAM, mirage_file_stream_stream_init));

static void mirage_file_stream_init (MirageFileStream *self)
{
    self->priv = MIRAGE_FILE_STREAM_GET_PRIVATE(self);

    /* Make sure all fields are empty */
    self->priv->input_stream = NULL;
    self->priv->output_stream = NULL;

    self->priv->stream = NULL;

    self->priv->filename = NULL;
}

static void mirage_file_stream_dispose (GObject *gobject)
{
    MirageFileStream *self = MIRAGE_FILE_STREAM(gobject);

    /* Unref stream object */
    if (self->priv->stream) {
        g_object_unref(self->priv->stream);
        self->priv->stream = NULL;
    }

    /* Chain up to the parent class */
    return G_OBJECT_CLASS(mirage_file_stream_parent_class)->dispose(gobject);
}


static void mirage_file_stream_finalize (GObject *gobject)
{
    MirageFileStream *self = MIRAGE_FILE_STREAM(gobject);

    /* Free filename */
    g_free(self->priv->filename);

    /* Chain up to the parent class */
    return G_OBJECT_CLASS(mirage_file_stream_parent_class)->finalize(gobject);
}

static void mirage_file_stream_class_init (MirageFileStreamClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = mirage_file_stream_dispose;
    gobject_class->finalize = mirage_file_stream_finalize;

    /* Register private structure */
    g_type_class_add_private(klass, sizeof(MirageFileStreamPrivate));
}

static void mirage_file_stream_stream_init (MirageStreamInterface *iface)
{
    iface->get_filename = mirage_file_stream_get_filename;
    iface->is_writable = mirage_file_stream_is_writable;
    iface->read = mirage_file_stream_read;
    iface->write = mirage_file_stream_write;
    iface->seek = mirage_file_stream_seek;
    iface->tell = mirage_file_stream_tell;
}
