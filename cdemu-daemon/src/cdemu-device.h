/*
 *  CDEmu daemon: Device object
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

#ifndef __CDEMU_DEVICE_H__
#define __CDEMU_DEVICE_H__

/* Forward declarations */
typedef struct _CdemuAudio CdemuAudio;
typedef struct _CdemuCommand CdemuCommand;


G_BEGIN_DECLS

/**********************************************************************\
 *                        CdemuDevice object                          *
\**********************************************************************/
#define CDEMU_TYPE_DEVICE            (cdemu_device_get_type())
#define CDEMU_DEVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), CDEMU_TYPE_DEVICE, CdemuDevice))
#define CDEMU_DEVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CDEMU_TYPE_DEVICE, CdemuDeviceClass))
#define CDEMU_IS_DEVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CDEMU_TYPE_DEVICE))
#define CDEMU_IS_DEVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CDEMU_TYPE_DEVICE))
#define CDEMU_DEVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CDEMU_TYPE_DEVICE, CdemuDeviceClass))

typedef struct _CdemuDevice           CdemuDevice;
typedef struct _CdemuDeviceClass      CdemuDeviceClass;
typedef struct _CdemuDevicePrivate    CdemuDevicePrivate;

struct _CdemuDevice
{
    MirageObject parent_instance;

    /*< private >*/
    CdemuDevicePrivate *priv;
};

struct _CdemuDeviceClass
{
    MirageObjectClass parent_class;

    /* Class members */
    guint signals[2]; /* Signals */
};


/* Used by CDEMU_TYPE_DEVICE */
GType cdemu_device_get_type (void);

/* Public API */
gboolean cdemu_device_initialize (CdemuDevice *self, gint number, gchar *ctl_device, gchar *audio_driver);

gint cdemu_device_get_device_number (CdemuDevice *self);

gboolean cdemu_device_get_status (CdemuDevice *self, gchar ***file_names);

gboolean cdemu_device_load_disc (CdemuDevice *self, gchar **file_names, GVariant *options, GError **error);
gboolean cdemu_device_unload_disc (CdemuDevice *self, GError **error);

GVariant *cdemu_device_get_option (CdemuDevice *self, gchar *option_name, GError **error);
gboolean cdemu_device_set_option (CdemuDevice *self, gchar *option_name, GVariant *option_value, GError **error);

gboolean cdemu_device_setup_mapping (CdemuDevice *self);
void cdemu_device_get_mapping (CdemuDevice *self, gchar **sr_device, gchar **sg_device);

gsize cdemu_device_get_kernel_io_buffer_size (CdemuDevice *self);

G_END_DECLS

#endif /* __CDEMU_DEVICE_H__ */
