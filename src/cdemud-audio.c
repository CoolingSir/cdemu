/*
 *  CDEmuD: Audio play object
 *  Copyright (C) 2006-2008 Rok Mandeljc
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

#include "cdemud.h"


/******************************************************************************\
 *                              Private structure                             *
\******************************************************************************/
#define CDEMUD_AUDIO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), CDEMUD_TYPE_AUDIO, CDEMUD_AudioPrivate))

typedef struct {    
    /* Thread */
    GThread *playback_thread;
    
    /* libao device */
    ao_device *device;
    
    /* Pointer to disc */
    GObject *disc;
    
    /* Sector */
    gint cur_sector;
    gint end_sector;

    gint *cur_sector_ptr;
    
    /* Status */
    gint status;
} CDEMUD_AudioPrivate;


static gpointer __cdemud_audio_playback_thread (gpointer data) {
    CDEMUD_Audio *self = data;
    CDEMUD_AudioPrivate *_priv = CDEMUD_AUDIO_GET_PRIVATE(self);

    CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: playback thread start\n", __func__);
    
    while (1) {
        /* Process sectors; we go over playing range, check sectors' type, keep 
           track of where we are and try to produce some sound. libao's play
           function should keep our timing */
        GObject *sector = NULL;
        GError *error = NULL;
        guint8 *tmp_buffer = NULL;
        gint tmp_len = 0;
        gint type = 0;
        
        /* Make playback thread interruptible (i.e. if status is changed, it's
           going to end */
        if (_priv->status != AUDIO_STATUS_PLAYING) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: playback interrupted\n", __func__);        
            goto end;
        }
        
        /* Check if we have already reached the end */
        if (_priv->cur_sector > _priv->end_sector) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: reached the end\n", __func__);
            _priv->status = AUDIO_STATUS_COMPLETED; /* Audio operation successfully completed */
            goto end;
        }
        
        /* Get sector */
        if (!mirage_disc_get_sector(MIRAGE_DISC(_priv->disc), _priv->cur_sector, &sector, &error)) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: failed to get sector 0x%X: %s\n", __func__, _priv->cur_sector, error->message);
            g_error_free(error);
            _priv->status = AUDIO_STATUS_ERROR; /* Audio operation stopped due to error */
            goto end;
        }
            
        /* This one covers both sector not being an audio one and sector changing 
           from audio to data one */
        mirage_sector_get_sector_type(MIRAGE_SECTOR(sector), &type, NULL);
        if (type != MIRAGE_MODE_AUDIO) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: non audio sector!\n", __func__);
            g_object_unref(sector); /* Unref here; we won't need it anymore... */
            _priv->status = AUDIO_STATUS_ERROR; /* Audio operation stopped due to error */
            goto end;
        }
        
        /* Play sector */
        mirage_sector_get_data(MIRAGE_SECTOR(sector), &tmp_buffer, &tmp_len, NULL);
        if (ao_play(_priv->device, (gchar *)tmp_buffer, tmp_len) == 0) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_ERROR, "%s: playback error!\n", __func__);
            _priv->status = AUDIO_STATUS_ERROR; /* Audio operation stopped due to error */
            goto end;
        }
        
        /* Save current position */
        if (_priv->cur_sector_ptr) {
            *_priv->cur_sector_ptr = _priv->cur_sector;
        }
        _priv->cur_sector++;
        
        g_object_unref(sector);
    }

end:
    CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: playback thread end\n", __func__);
    return NULL;
}

static gboolean __cdemud_audio_start_playing (CDEMUD_Audio *self, GError **error) {
    CDEMUD_AudioPrivate *_priv = CDEMUD_AUDIO_GET_PRIVATE(self);
    
    /* Set the status */
    _priv->status = AUDIO_STATUS_PLAYING;
    
    /* Start the playback thread; thread must be joinable, so we can wait for it
       to end */
    _priv->playback_thread = g_thread_create(__cdemud_audio_playback_thread, self, TRUE, NULL);
    
    return TRUE;
}

static gboolean __cdemud_audio_stop_playing (CDEMUD_Audio *self, gint status, GError **error) {
    CDEMUD_AudioPrivate *_priv = CDEMUD_AUDIO_GET_PRIVATE(self);
    
    /* We can't tell whether we're stopped or paused, so the upper layer needs
       to provide us appropriate status */
    _priv->status = status;
    
    /* Wait for the thread to finish */
    CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: waiting for thread to finish\n", __func__);
    g_thread_join(_priv->playback_thread);
    _priv->playback_thread = NULL;
    CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: thread finished\n", __func__);
    
    return TRUE;
}


/******************************************************************************\
 *                                 Public API                                 *
\******************************************************************************/
gboolean cdemud_audio_initialize (CDEMUD_Audio *self, gchar *driver, gint *cur_sector_ptr, GError **error) {
    CDEMUD_AudioPrivate *_priv = CDEMUD_AUDIO_GET_PRIVATE(CDEMUD_AUDIO(self));
    gint driver_id = 0;
    ao_sample_format format;
        
    _priv->cur_sector_ptr = cur_sector_ptr;
    
    _priv->status = AUDIO_STATUS_NOSTATUS;
        
    /* Initialize libao */
    ao_initialize();
	
	/* Get driver ID */
    if (!strcmp(driver, "default")) {
        driver_id = ao_default_driver_id();
    } else {
        driver_id = ao_driver_id(driver);
    }
    
    if (driver_id == -1) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_ERROR, "%s: cannot find driver '%s'!\n", __func__, driver);
        return FALSE;
    }
    
	format.bits = 16;
	format.channels = 2;
	format.rate = 44100;
	format.byte_format = AO_FMT_LITTLE;
	
	/* Open driver -- */
	_priv->device = ao_open_live(driver_id, &format, NULL /* no options */);
	if (_priv->device == NULL) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_ERROR, "%s: failed to open audio device (driver: '%s')!\n", __func__, driver);
		return FALSE;
	}
    
    return TRUE;
}

gboolean cdemud_audio_start (CDEMUD_Audio *self, gint start, gint end, GObject *disc, GError **error) {
    CDEMUD_AudioPrivate *_priv = CDEMUD_AUDIO_GET_PRIVATE(self);
    gboolean succeeded = TRUE;
    
    /* Lock */
   // g_static_mutex_lock(&_priv->mutex);
    
    /* Play is valid only if we're not playing already or paused */
    if (_priv->status != AUDIO_STATUS_PLAYING && _priv->status != AUDIO_STATUS_PAUSED) {
        /* Set start and end sector, and disc... We should have been stopped properly
           before, which means we don't have to unref the previous disc reference */
        _priv->cur_sector = start;
        _priv->end_sector = end;
        _priv->disc = disc;
        
        /* Reference disc for the time of playing */
        g_object_ref(_priv->disc);
        
        CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: starting playback (0x%X->0x%X)...\n", __func__, _priv->cur_sector, _priv->end_sector);
        if (!__cdemud_audio_start_playing(CDEMUD_AUDIO(self), error)) {
            /* Free disc reference */
            g_object_unref(_priv->disc);
            _priv->disc = NULL;
            
            CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: failed to start playback!\n", __func__);
            succeeded = FALSE;
            goto end;
        }
    } else {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: play called when paused or already playing!\n", __func__);
        cdemud_error(CDEMUD_E_AUDIOINVALIDSTATE, error);
        succeeded = FALSE;
        goto end;
    }
    
end:
    /* Unlock */
  //  g_static_mutex_unlock(&_priv->mutex);
    
    return succeeded;
}

gboolean cdemud_audio_resume (CDEMUD_Audio *self, GError **error) {
    CDEMUD_AudioPrivate *_priv = CDEMUD_AUDIO_GET_PRIVATE(self);
    gboolean succeeded = TRUE;
    
    /* Lock */
   // g_static_mutex_lock(&_priv->mutex);
    
    /* Resume is valid only if we're paused */
    if (_priv->status == AUDIO_STATUS_PAUSED) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: resuming playback (0x%X->0x%X)...\n", __func__);
        if (!__cdemud_audio_start_playing(CDEMUD_AUDIO(self), error)) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: failed to start playback!\n", __func__);
            succeeded = FALSE;
            goto end;
        }
    } else {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: resume called when not paused!\n", __func__);
        cdemud_error(CDEMUD_E_AUDIOINVALIDSTATE, error);
        succeeded = FALSE;
        goto end;
    }
    
end:
    /* Unlock */
   // g_static_mutex_unlock(&_priv->mutex);
    
    return succeeded;
}

gboolean cdemud_audio_pause (CDEMUD_Audio *self, GError **error) {
    CDEMUD_AudioPrivate *_priv = CDEMUD_AUDIO_GET_PRIVATE(self);
    gboolean succeeded = TRUE;
    
    /* Pause is valid only if we are playing */
    if (_priv->status == AUDIO_STATUS_PLAYING) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: pausing playback...\n", __func__);
        
        if (!__cdemud_audio_stop_playing(CDEMUD_AUDIO(self), AUDIO_STATUS_PAUSED, error)) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: failed to pause playback!\n", __func__);
            succeeded = FALSE;
            goto end;
        }
    } else {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: pause called when not playing!\n", __func__);
        cdemud_error(CDEMUD_E_AUDIOINVALIDSTATE, error);
        succeeded = FALSE;
        goto end;
    }
    
end:    
    return succeeded;
}

gboolean cdemud_audio_stop (CDEMUD_Audio *self, GError **error) {
    CDEMUD_AudioPrivate *_priv = CDEMUD_AUDIO_GET_PRIVATE(self);
    gboolean succeeded = TRUE;
    
    /* Stop is valid only if we are playing or paused */
    if (_priv->status == AUDIO_STATUS_PLAYING || _priv->status == AUDIO_STATUS_PAUSED) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: stopping playback...\n", __func__);
        
        if (__cdemud_audio_stop_playing(CDEMUD_AUDIO(self), AUDIO_STATUS_NOSTATUS, error)) {
            /* Release disc reference */
            g_object_unref(_priv->disc);
            _priv->disc = NULL;
        } else {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: failed to stop playback!\n", __func__);
            succeeded = FALSE;
            goto end;
        }
    } else {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_DEV_AUDIOPLAY, "%s: stop called when not playing nor paused!\n", __func__);
        cdemud_error(CDEMUD_E_AUDIOINVALIDSTATE, error);
        succeeded = FALSE;
        goto end;
    }

end:
    return succeeded;
}

gboolean cdemud_audio_get_status (CDEMUD_Audio *self, gint *status, GError **error) {
    CDEMUD_AudioPrivate *_priv = CDEMUD_AUDIO_GET_PRIVATE(self);
    
    /* Return status */
    *status = _priv->status;
    
    return TRUE;
}

/******************************************************************************\
 *                                 Object init                                *
\******************************************************************************/
/* Our parent class */
static MIRAGE_ObjectClass *parent_class = NULL;

static void __cdemud_audio_finalize (GObject *obj) {
    CDEMUD_Audio *self = CDEMUD_AUDIO(obj);                                                
    CDEMUD_AudioPrivate *_priv = CDEMUD_AUDIO_GET_PRIVATE(self);
        
    /* Close device */
    ao_close(_priv->device);
    ao_shutdown();    
    
    /* Chain up to the parent class */
    return G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void __cdemud_audio_class_init (gpointer g_class, gpointer g_class_data) {
    GObjectClass *class_gobject = G_OBJECT_CLASS(g_class);
    CDEMUD_AudioClass *klass = CDEMUD_AUDIO_CLASS(g_class);
    
    /* Set parent class */
    parent_class = g_type_class_peek_parent(klass);
    
    /* Register private structure */
    g_type_class_add_private(klass, sizeof(CDEMUD_AudioPrivate));
    
    /* Initialize GObject members */
    class_gobject->finalize = __cdemud_audio_finalize;
    
    return;
}

GType cdemud_audio_get_type (void) {
    static GType type = 0;
    if (type == 0) {
        static const GTypeInfo info = {
            sizeof(CDEMUD_AudioClass),
            NULL,   /* base_init */
            NULL,   /* base_finalize */
            __cdemud_audio_class_init,   /* class_init */
            NULL,   /* class_finalize */
            NULL,   /* class_data */
            sizeof(CDEMUD_Audio),
            0,      /* n_preallocs */
            NULL    /* instance_init */
        };
        
        type = g_type_register_static(MIRAGE_TYPE_OBJECT, "CDEMUD_Audio", &info, 0);
    }
    
    return type;
}
