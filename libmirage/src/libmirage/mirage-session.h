/*
 *  libMirage: Session object
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

#ifndef __MIRAGE_SESSION_H__
#define __MIRAGE_SESSION_H__

/* Forward declarations */
typedef struct _MirageTrack MirageTrack;
typedef struct _MirageLanguage MirageLanguage;


G_BEGIN_DECLS

/**
 * MirageSessionTypes:
 * @MIRAGE_SESSION_CD_DA: CD AUDIO
 * @MIRAGE_SESSION_CD_ROM: CD-ROM
 * @MIRAGE_SESSION_CD_I: CD-I
 * @MIRAGE_SESSION_CD_ROM_XA: CD-ROM XA
 *
 * <para>
 * Session types
 * </para>
 **/
typedef enum _MirageSessionTypes
{
    MIRAGE_SESSION_CD_DA     = 0x00,
    MIRAGE_SESSION_CD_ROM    = 0x00,
    MIRAGE_SESSION_CD_I      = 0x10,
    MIRAGE_SESSION_CD_ROM_XA = 0x20,
} MirageSessionTypes;


/**********************************************************************\
 *                         MirageSession object                       *
\**********************************************************************/
#define MIRAGE_TYPE_SESSION            (mirage_session_get_type())
#define MIRAGE_SESSION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), MIRAGE_TYPE_SESSION, MirageSession))
#define MIRAGE_SESSION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), MIRAGE_TYPE_SESSION, MirageSessionClass))
#define MIRAGE_IS_SESSION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), MIRAGE_TYPE_SESSION))
#define MIRAGE_IS_SESSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MIRAGE_TYPE_SESSION))
#define MIRAGE_SESSION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), MIRAGE_TYPE_SESSION, MirageSessionClass))

typedef struct _MirageSession          MirageSession;
typedef struct _MirageSessionClass     MirageSessionClass;
typedef struct _MirageSessionPrivate   MirageSessionPrivate;

/**
 * MirageSession:
 *
 * <para>
 * Contains private data only, and should be accessed using the functions below.
 * </para>
 **/
struct _MirageSession
{
    MirageObject parent_instance;

    /*< private >*/
    MirageSessionPrivate *priv;
};

struct _MirageSessionClass
{
    MirageObjectClass parent_class;
};

/* Used by MIRAGE_TYPE_SESSION */
GType mirage_session_get_type (void);

/* Session type */
void mirage_session_set_session_type (MirageSession *self, MirageSessionTypes type);
MirageSessionTypes mirage_session_get_session_type (MirageSession *self);

/* Layout */
void mirage_session_layout_set_session_number (MirageSession *self, gint number);
gint mirage_session_layout_get_session_number (MirageSession *self);
void mirage_session_layout_set_first_track (MirageSession *self, gint first_track);
gint mirage_session_layout_get_first_track (MirageSession *self);
void mirage_session_layout_set_start_sector (MirageSession *self, gint start_sector);
gint mirage_session_layout_get_start_sector (MirageSession *self);
gint mirage_session_layout_get_length (MirageSession *self);

/* Convenience functions for setting/getting length of session's lead-out */
void mirage_session_set_leadout_length (MirageSession *self, gint length);
gint mirage_session_get_leadout_length (MirageSession *self);

/* Tracks handling */
gint mirage_session_get_number_of_tracks (MirageSession *self);
void mirage_session_add_track_by_index (MirageSession *self, gint index, MirageTrack *track);
gboolean mirage_session_add_track_by_number (MirageSession *self, gint number, MirageTrack *track, GError **error);
gboolean mirage_session_remove_track_by_index (MirageSession *self, gint index, GError **error);
gboolean mirage_session_remove_track_by_number (MirageSession *self, gint number, GError **error);
void mirage_session_remove_track_by_object (MirageSession *self, MirageTrack *track);
MirageTrack *mirage_session_get_track_by_index (MirageSession *self, gint index, GError **error);
MirageTrack *mirage_session_get_track_by_number (MirageSession *self, gint number, GError **error);
MirageTrack *mirage_session_get_track_by_address (MirageSession *self, gint address, GError **error);
gboolean mirage_session_enumerate_tracks (MirageSession *self, MirageCallbackFunction func, gpointer user_data);
MirageTrack *mirage_session_get_track_before (MirageSession *self, MirageTrack *track, GError **error);
MirageTrack *mirage_session_get_track_after (MirageSession *self, MirageTrack *track, GError **error);

/* Languages (CD-Text) handling */
gint mirage_session_get_number_of_languages (MirageSession *self);
gboolean mirage_session_add_language (MirageSession *self, gint code, MirageLanguage *language, GError **error);
gboolean mirage_session_remove_language_by_index (MirageSession *self, gint index, GError **error);
gboolean mirage_session_remove_language_by_code (MirageSession *self, gint code, GError **error);
void mirage_session_remove_language_by_object (MirageSession *self, MirageLanguage *language);
MirageLanguage *mirage_session_get_language_by_index (MirageSession *self, gint index, GError **error);
MirageLanguage *mirage_session_get_language_by_code (MirageSession *self, gint code, GError **error);
gboolean mirage_session_enumerate_languages (MirageSession *self, MirageCallbackFunction func, gpointer user_data);

/* Direct CD-Text handling */
gboolean mirage_session_set_cdtext_data (MirageSession *self, guint8 *data, gint len, GError **error);
gboolean mirage_session_get_cdtext_data (MirageSession *self, guint8 **data, gint *len, GError **error);

/* Two nice convenience functions */
MirageSession *mirage_session_get_prev (MirageSession *self, GError **error);
MirageSession *mirage_session_get_next (MirageSession *self, GError **error);

G_END_DECLS

#endif /* __MIRAGE_SESSION_H__ */
