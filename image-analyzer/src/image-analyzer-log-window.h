/*
 *  Image Analyzer: Parser log window
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

#ifndef __IMAGE_ANALYZER_LOG_WINDOW_H__
#define __IMAGE_ANALYZER_LOG_WINDOW_H__


G_BEGIN_DECLS


#define IMAGE_ANALYZER_TYPE_LOG_WINDOW            (image_analyzer_log_window_get_type())
#define IMAGE_ANALYZER_LOG_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGE_ANALYZER_TYPE_LOG_WINDOW, ImageAnalyzerLogWindow))
#define IMAGE_ANALYZER_LOG_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), IMAGE_ANALYZER_TYPE_LOG_WINDOW, ImageAnalyzerLogWindowClass))
#define IMAGE_ANALYZER_IS_LOG_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGE_ANALYZER_TYPE_LOG_WINDOW))
#define IMAGE_ANALYZER_IS_LOG_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), IMAGE_ANALYZER_TYPE_LOG_WINDOW))
#define IMAGE_ANALYZER_LOG_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), IMAGE_ANALYZER_TYPE_LOG_WINDOW, ImageAnalyzerLogWindowClass))

typedef struct _ImageAnalyzerLogWindow        ImageAnalyzerLogWindow;
typedef struct _ImageAnalyzerLogWindowClass   ImageAnalyzerLogWindowClass;
typedef struct _ImageAnalyzerLogWindowPrivate ImageAnalyzerLogWindowPrivate;

struct _ImageAnalyzerLogWindow
{
    GtkWindow parent_instance;

    /*< private >*/
    ImageAnalyzerLogWindowPrivate *priv;
};

struct _ImageAnalyzerLogWindowClass
{
    GtkWindowClass parent_class;
};


/* Used by IMAGE_ANALYZER_TYPE_LOG_WINDOW */
GType image_analyzer_log_window_get_type (void);

/* Public API */
void image_analyzer_log_window_clear_log (ImageAnalyzerLogWindow *self);
void image_analyzer_log_window_append_to_log (ImageAnalyzerLogWindow *self, const gchar *message);

gchar *image_analyzer_log_window_get_log_text (ImageAnalyzerLogWindow *self);

void image_analyzer_log_window_set_debug_to_stdout (ImageAnalyzerLogWindow *self, gboolean enabled);

G_END_DECLS

#endif /* __IMAGE_ANALYZER_LOG_WINDOW_H__ */
