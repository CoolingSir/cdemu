/*
 *  MIRAGE Image Analyzer: Disc topology window
 *  Copyright (C) 2007 Rok Mandeljc
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

#ifndef __IMAGE_ANALYZER_DISC_TOPOLOGY_H__
#define __IMAGE_ANALYZER_DISC_TOPOLOGY_H__


G_BEGIN_DECLS


#define IMAGE_ANALYZER_TYPE_DISC_TOPOLOGY            (image_analyzer_disc_topology_get_type())
#define IMAGE_ANALYZER_DISC_TOPOLOGY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGE_ANALYZER_TYPE_DISC_TOPOLOGY, IMAGE_ANALYZER_DiscTopology))
#define IMAGE_ANALYZER_DISC_TOPOLOGY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), IMAGE_ANALYZER_TYPE_DISC_TOPOLOGY, IMAGE_ANALYZER_DiscTopologyClass))
#define IMAGE_ANALYZER_IS_DISC_TOPOLOGY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGE_ANALYZER_TYPE_DISC_TOPOLOGY))
#define IMAGE_ANALYZER_IS_DISC_TOPOLOGY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), IMAGE_ANALYZER_TYPE_DISC_TOPOLOGY))
#define IMAGE_ANALYZER_DISC_TOPOLOGY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), IMAGE_ANALYZER_TYPE_DISC_TOPOLOGY, IMAGE_ANALYZER_DiscTopologyClass))

typedef struct {
    GtkWindow parent;
} IMAGE_ANALYZER_DiscTopology;

typedef struct {
    GtkWindowClass parent;
} IMAGE_ANALYZER_DiscTopologyClass;

/* Used by IMAGE_ANALYZER_TYPE_DISC_TOPOLOGY */
GType image_analyzer_disc_topology_get_type (void);


/******************************************************************************\
 *                                 Public API                                 *
\******************************************************************************/
gboolean image_analyzer_disc_topology_create (IMAGE_ANALYZER_DiscTopology *self, GObject *disc, GError **error);
gboolean image_analyzer_disc_topology_clear (IMAGE_ANALYZER_DiscTopology *self, GError **error);


G_END_DECLS

#endif /* __IMAGE_ANALYZER_DISC_TOPOLOGY_H__ */
