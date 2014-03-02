/*
 *  libMirage: XZ filter: filter stream
 *  Copyright (C) 2012-2014 Rok Mandeljc
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

#ifndef __FILTER_XZ_FILTER_STREAM_H__
#define __FILTER_XZ_FILTER_STREAM_H__


G_BEGIN_DECLS

#define MIRAGE_TYPE_FILTER_STREAM_XZ            (mirage_filter_stream_xz_get_type())
#define MIRAGE_FILTER_STREAM_XZ(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), MIRAGE_TYPE_FILTER_STREAM_XZ, MirageFilterStreamXz))
#define MIRAGE_FILTER_STREAM_XZ_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), MIRAGE_TYPE_FILTER_STREAM_XZ, MirageFilterStreamXzClass))
#define MIRAGE_IS_FILTER_STREAM_XZ(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), MIRAGE_TYPE_FILTER_STREAM_XZ))
#define MIRAGE_IS_FILTER_STREAM_XZ_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MIRAGE_TYPE_FILTER_STREAM_XZ))
#define MIRAGE_FILTER_STREAM_XZ_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), MIRAGE_TYPE_FILTER_STREAM_XZ, MirageFilterStreamXzClass))

typedef struct _MirageFilterStreamXz        MirageFilterStreamXz;
typedef struct _MirageFilterStreamXzClass   MirageFilterStreamXzClass;
typedef struct _MirageFilterStreamXzPrivate MirageFilterStreamXzPrivate;

struct _MirageFilterStreamXz
{
    MirageFilterStream parent_instance;

    /*< private >*/
    MirageFilterStreamXzPrivate *priv;
};

struct _MirageFilterStreamXzClass
{
    MirageFilterStreamClass parent_class;
};

/* Used by MIRAGE_TYPE_FILTER_STREAM_XZ */
GType mirage_filter_stream_xz_get_type (void);
void mirage_filter_stream_xz_type_register (GTypeModule *type_module);

G_END_DECLS

#endif /* __FILTER_XZ_FILTER_STREAM_H__ */
