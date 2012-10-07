/*
 *  libMirage: Main header
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


#ifndef __MIRAGE_H__
#define __MIRAGE_H__

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gmodule.h>

#include <gio/gio.h>

#include <math.h>
#include <string.h>

#include "mirage-object.h"

#include "mirage-cdtext-coder.h"
#include "mirage-debug.h"
#include "mirage-disc.h"
#include "mirage-disc-structures.h"
#include "mirage-error.h"
#include "mirage-file-filter.h"
#include "mirage-fragment.h"
#include "mirage-index.h"
#include "mirage-language.h"
#include "mirage-parser.h"
#include "mirage-plugin.h"
#include "mirage-sector.h"
#include "mirage-session.h"
#include "mirage-track.h"
#include "mirage-utils.h"
#include "mirage-version.h"

G_BEGIN_DECLS


/**
 * MirageEnumParserInfoCallback:
 * @info: (in): parser info
 * @user_data: (in) (closure): user data passed to enumeration function
 *
 * <para>
 * Callback function type used with mirage_enumerate_parsers().
 * A pointer to parser information structure is stored in @info; the
 * structure belongs to the parser object and should not be modified.
 * @user_data is user data passed to enumeration function.
 * </para>
 *
 * Returns: %TRUE on success, otherwise %FALSE
 **/
typedef gboolean (*MirageEnumParserInfoCallback) (const MirageParserInfo *info, gpointer user_data);

/**
 * MirageEnumFragmentInfoCallback:
 * @info: (in): fragment info
 * @user_data: (in) (closure): user data passed to enumeration function
 *
 * <para>
 * Callback function type used with mirage_enumerate_fragments().
 * A pointer to fragment information structure is stored in @info; the
 * structure belongs to the fragment object and should not be modified.
 * @user_data is user data passed to enumeration function.
 * </para>
 *
 * Returns: %TRUE on success, otherwise %FALSE
 **/
typedef gboolean (*MirageEnumFragmentInfoCallback) (const MirageFragmentInfo *info, gpointer user_data);

/**
 * MirageEnumFileFilterInfoCallback:
 * @info: (in): file filter info
 * @user_data: (in) (closure): user data passed to enumeration function
 *
 * <para>
 * Callback function type used with mirage_enumerate_file_filters().
 * A pointer to file filter information structure is stored in @info; the
 * structure belongs to the file filter object and should not be modified.
 * @user_data is user data passed to enumeration function.
 * </para>
 *
 * Returns: %TRUE on success, otherwise %FALSE
 **/
typedef gboolean (*MirageEnumFileFilterInfoCallback) (const MirageFileFilterInfo *info, gpointer user_data);

/**
 * MiragePasswordFunction:
 * @user_data: (in) (closure): user data passed to password function
 *
 * <para>
 * Password function type used in libMirage's to obtain password for encrypted
 * images. A password function needs to be set to libMirage via
 * mirage_set_password_function(), along with @user_data that the password
 * function should be called with.
 * </para>
 *
 * Returns: password string on success, otherwise %NULL. Password string should
 * be a copy, allocated via function such as g_strdup(), and will be freed after
 * it is used.
 **/
typedef gchar *(*MiragePasswordFunction) (gpointer user_data);

/**
 * MirageDebugMask:
 * @name: name
 * @value: value
 *
 * <para>
 * Structure containing debug mask information.
 * </para>
 **/
typedef struct {
    gchar *name;
    gint value;
} MirageDebugMask;


/* *** libMirage API *** */
gboolean mirage_initialize (GError **error);
gboolean mirage_shutdown (GError **error);

gboolean mirage_set_password_function (MiragePasswordFunction func, gpointer user_data, GError **error);
gchar *mirage_obtain_password (GError **error);

MirageDisc *mirage_create_disc (gchar **filenames, GObject *debug_context, GHashTable *params, GError **error);
MirageFragment *mirage_create_fragment (GType fragment_interface, GInputStream *stream, GObject *debug_context, GError **error);
GInputStream *mirage_create_file_stream (const gchar *filename, GObject *debug_context, GError **error);

const gchar *mirage_get_file_stream_filename (GInputStream *stream);

gboolean mirage_enumerate_parsers (MirageEnumParserInfoCallback func, gpointer user_data, GError **error);
gboolean mirage_enumerate_fragments (MirageEnumFragmentInfoCallback func, gpointer user_data, GError **error);
gboolean mirage_enumerate_file_filters (MirageEnumFileFilterInfoCallback func, gpointer user_data, GError **error);

gboolean mirage_get_supported_debug_masks (const MirageDebugMask **masks, gint *num_masks, GError **error);

G_END_DECLS

#endif /* __MIRAGE_H__ */
