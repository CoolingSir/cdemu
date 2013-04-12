/*
 *  Image Analyzer: Sector read window
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __IMAGE_ANALYZER_SECTOR_READ_H__
#define __IMAGE_ANALYZER_SECTOR_READ_H__


G_BEGIN_DECLS


#define IMAGE_ANALYZER_TYPE_SECTOR_READ            (image_analyzer_sector_read_get_type())
#define IMAGE_ANALYZER_SECTOR_READ(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGE_ANALYZER_TYPE_SECTOR_READ, ImageAnalyzerSectorRead))
#define IMAGE_ANALYZER_SECTOR_READ_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), IMAGE_ANALYZER_TYPE_SECTOR_READ, ImageAnalyzerSectorReadClass))
#define IMAGE_ANALYZER_IS_SECTOR_READ(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGE_ANALYZER_TYPE_SECTOR_READ))
#define IMAGE_ANALYZER_IS_SECTOR_READ_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), IMAGE_ANALYZER_TYPE_SECTOR_READ))
#define IMAGE_ANALYZER_SECTOR_READ_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), IMAGE_ANALYZER_TYPE_SECTOR_READ, ImageAnalyzerSectorReadClass))

typedef struct _ImageAnalyzerSectorRead           ImageAnalyzerSectorRead;
typedef struct _ImageAnalyzerSectorReadClass      ImageAnalyzerSectorReadClass;
typedef struct _ImageAnalyzerSectorReadPrivate    ImageAnalyzerSectorReadPrivate;

struct _ImageAnalyzerSectorRead {
    GtkWindow parent_instance;

    /*< private >*/
    ImageAnalyzerSectorReadPrivate *priv;
};

struct _ImageAnalyzerSectorReadClass {
    GtkWindowClass parent_class;
};


/* Used by IMAGE_ANALYZER_TYPE_SECTOR_READ */
GType image_analyzer_sector_read_get_type (void);

/* Public API */
void image_analyzer_sector_read_set_disc (ImageAnalyzerSectorRead *self, MirageDisc *disc);


G_END_DECLS

#endif /* __IMAGE_ANALYZER_SECTOR_READ_H__ */
