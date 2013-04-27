/*
 *  libMirage: Apple Resource Fork routines.
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

#ifndef __RESOURCE_FORK_H__
#define __RESOURCE_FORK_H__

G_BEGIN_DECLS

#pragma pack(1)
typedef struct {
    guint32 data_offset;
    guint32 map_offset;
    guint32 data_length;
    guint32 map_length;
} rsrc_raw_header_t; /* length: 16 bytes */

typedef struct {
    rsrc_raw_header_t header_copy;

    guint32 handle_next_map; /* reserved */
    guint16 file_ref_num;
    guint16 res_fork_attrs;
    guint16 type_list_offset;
    guint16 name_list_offset;
    guint16 num_types_minus_one;
} rsrc_raw_map_t; /* length: 30 bytes */

typedef struct {
    union {
        gchar   type[4];
        guint32 type_as_int;
    };
    guint16 num_refs_minus_one;
    guint16 ref_offset;
} rsrc_raw_type_t; /* length: 8 bytes */

typedef struct {
    gint16  id;
    gint16  name_offset; /* -1 means no name */
    guint8  attrs;
    guint8  data_offset[3];
    guint32 handle; /* reserved */
} rsrc_raw_ref_t; /* length: 12 bytes */
#pragma pack()

typedef struct {
    gint16  id;
    guint8  attrs;
    GString *name;
    gchar   *data;
    guint32 data_length;
} rsrc_ref_t;

typedef struct {
    union {
        gchar   type[4];
        guint32 type_as_int;
    };
    guint      num_refs;
    rsrc_ref_t *ref_list;
} rsrc_type_t;

typedef struct {
    guint16     file_ref_num;
    guint16     res_fork_attrs;
    guint       num_types;
    rsrc_type_t *type_list;
} rsrc_fork_t;

/* Forward declarations */
rsrc_fork_t *rsrc_fork_read_xml(const gchar *xml_data, gssize xml_length);
rsrc_fork_t *rsrc_fork_read_binary(gchar *raw_data);
gboolean rsrc_fork_free(rsrc_fork_t *rsrc_fork);

G_END_DECLS

#endif /* __RESOURCE_FORK_H__ */
