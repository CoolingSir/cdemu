/*
 *  CDEmuD: Audio play object
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

#include "cdemud.h"
#include "cdemud-audio-private.h"

#define __debug__ "AudioPlay"


/**********************************************************************\
 *                          Playback functions                        *
\**********************************************************************/
static gpointer cdemud_audio_playback_thread (CDEMUD_Audio *self)
{
    /* Open audio device */
    CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: opening audio device\n", __debug__);
    self->priv->device = ao_open_live(self->priv->driver_id, &self->priv->format, NULL /* no options */);
    if (self->priv->device == NULL) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_WARNING, "%s: failed to open audio device!\n", __debug__);
        return NULL;
    }
    
    /* Activate null driver hack, if needed (otherwise disable it) */
    if (self->priv->driver_id == ao_driver_id("null")) {
        self->priv->null_hack = TRUE;
    } else {
        self->priv->null_hack = FALSE;
    }
    
    CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: playback thread start\n", __debug__);
    
    while (1) {
        /* Process sectors; we go over playing range, check sectors' type, keep 
           track of where we are and try to produce some sound. libao's play
           function should keep our timing */
        GObject *sector = NULL;
        GError *error = NULL;
        const guint8 *tmp_buffer = NULL;
        gint tmp_len = 0;
        gint type = 0;
        
        /* Make playback thread interruptible (i.e. if status is changed, it's
           going to end */
        if (self->priv->status != AUDIO_STATUS_PLAYING) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: playback thread interrupted\n", __debug__);
            break;
        }
        
        /* Check if we have already reached the end */
        if (self->priv->cur_sector > self->priv->end_sector) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: playback thread reached the end\n", __debug__);
            self->priv->status = AUDIO_STATUS_COMPLETED; /* Audio operation successfully completed */
            break;
        }
        
        
        /*** Lock device mutex ***/
        g_mutex_lock(self->priv->device_mutex);
        
        /* Get sector */
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: playing sector %d (0x%X)\n", __debug__, self->priv->cur_sector, self->priv->cur_sector);
        if (!mirage_disc_get_sector(MIRAGE_DISC(self->priv->disc), self->priv->cur_sector, &sector, &error)) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: failed to get sector 0x%X: %s\n", __debug__, self->priv->cur_sector, error->message);
            g_error_free(error);
            self->priv->status = AUDIO_STATUS_ERROR; /* Audio operation stopped due to error */
            g_mutex_unlock(self->priv->device_mutex);
            break;
        }
            
        /* This one covers both sector not being an audio one and sector changing 
           from audio to data one */
        mirage_sector_get_sector_type(MIRAGE_SECTOR(sector), &type, NULL);
        if (type != MIRAGE_MODE_AUDIO) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: non-audio sector!\n", __debug__);
            g_object_unref(sector); /* Unref here; we won't need it anymore... */
            self->priv->status = AUDIO_STATUS_ERROR; /* Audio operation stopped due to error */
            g_mutex_unlock(self->priv->device_mutex);
            break;
        }
        
        /* Save current position */
        if (self->priv->cur_sector_ptr) {
            *self->priv->cur_sector_ptr = self->priv->cur_sector;
        }
        self->priv->cur_sector++;
        
        /*** Unlock device mutex ***/
        g_mutex_unlock(self->priv->device_mutex);
        
        
        /* Play sector */
        mirage_sector_get_data(MIRAGE_SECTOR(sector), &tmp_buffer, &tmp_len, NULL);
        if (ao_play(self->priv->device, (gchar *)tmp_buffer, tmp_len) == 0) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_ERROR, "%s: playback error!\n", __debug__);
            self->priv->status = AUDIO_STATUS_ERROR; /* Audio operation stopped due to error */
            break;
        }
        
        /* Hack: account for null driver's behaviour; for other libao drivers, ao_play
           seems to return after the data is played, which is what we rely on for our
           timing. However, null driver, as it has no device to write to, returns
           immediately. Until this is fixed in libao, we'll have to emulate the delay
           ourselves */
        if (self->priv->null_hack) {
            g_usleep(1*G_USEC_PER_SEC/75); /* One sector = 1/75th of second */
        }
        
        g_object_unref(sector);
    }

    CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: playback thread end\n", __debug__);
    
    /* Close audio device */
    CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: closing audio device\n", __debug__);
    ao_close(self->priv->device);
    self->priv->device = 0;
    
    return NULL;
}

static gboolean cdemud_audio_start_playing (CDEMUD_Audio *self, GError **error G_GNUC_UNUSED)
{   
    /* Set the status */
    self->priv->status = AUDIO_STATUS_PLAYING;
    
    /* Start the playback thread; thread must be joinable, so we can wait for it
       to end */
    self->priv->playback_thread = g_thread_create((GThreadFunc)cdemud_audio_playback_thread, self, TRUE, NULL);
    
    return TRUE;
}

static gboolean cdemud_audio_stop_playing (CDEMUD_Audio *self, gint status, GError **error G_GNUC_UNUSED)
{   
    /* We can't tell whether we're stopped or paused, so the upper layer needs
       to provide us appropriate status */
    self->priv->status = status;
    
    /* Wait for the thread to finish */
    if (self->priv->playback_thread) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: waiting for thread to finish\n", __debug__);
        g_thread_join(self->priv->playback_thread);
        self->priv->playback_thread = NULL;
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: thread finished\n", __debug__);
    }
    
    return TRUE;
}


/**********************************************************************\
 *                                 Public API                         *
\**********************************************************************/
gboolean cdemud_audio_initialize (CDEMUD_Audio *self, gchar *driver, gint *cur_sector_ptr, GMutex *device_mutex_ptr, GError **error G_GNUC_UNUSED)
{
    self->priv->cur_sector_ptr = cur_sector_ptr;
    self->priv->device_mutex = device_mutex_ptr;
    
    self->priv->status = AUDIO_STATUS_NOSTATUS;
	
    /* Get driver ID */
    if (!strcmp(driver, "default")) {
        self->priv->driver_id = ao_default_driver_id();
    } else {
        self->priv->driver_id = ao_driver_id(driver);
    }
    
    if (self->priv->driver_id == -1) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_WARNING, "%s: cannot find driver '%s', using 'null' instead!\n", __debug__, driver);
        self->priv->driver_id = ao_driver_id("null");
    }
    
    /* Set the audio format */    
    self->priv->format.bits = 16;
    self->priv->format.channels = 2;
    self->priv->format.rate = 44100;
    self->priv->format.byte_format = AO_FMT_LITTLE;

    /* *Don't* open the device here; we'll do it when we actually start playing */

    return TRUE;
}

gboolean cdemud_audio_start (CDEMUD_Audio *self, gint start, gint end, GObject *disc, GError **error)
{
    gboolean succeeded = TRUE;
    
    /* Lock */
   // g_static_mutex_lock(&self->priv->mutex);
    
    /* Play is valid only if we're not playing already or paused */
    if (self->priv->status != AUDIO_STATUS_PLAYING && self->priv->status != AUDIO_STATUS_PAUSED) {
        /* Set start and end sector, and disc... We should have been stopped properly
           before, which means we don't have to unref the previous disc reference */
        self->priv->cur_sector = start;
        self->priv->end_sector = end;
        self->priv->disc = disc;
        
        /* Reference disc for the time of playing */
        g_object_ref(self->priv->disc);
        
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: starting playback (0x%X->0x%X)...\n", __debug__, self->priv->cur_sector, self->priv->end_sector);
        if (!cdemud_audio_start_playing(self, error)) {
            /* Free disc reference */
            g_object_unref(self->priv->disc);
            self->priv->disc = NULL;
            
            CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: failed to start playback!\n", __debug__);
            succeeded = FALSE;
            goto end;
        }
    } else {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: play called when paused or already playing!\n", __debug__);
        cdemud_error(CDEMUD_E_AUDIOINVALIDSTATE, error);
        succeeded = FALSE;
        goto end;
    }
    
end:
    /* Unlock */
  //  g_static_mutex_unlock(&self->priv->mutex);
    
    return succeeded;
}

gboolean cdemud_audio_resume (CDEMUD_Audio *self, GError **error)
{
    gboolean succeeded = TRUE;
    
    /* Lock */
   // g_static_mutex_lock(&self->priv->mutex);
    
    /* Resume is valid only if we're paused */
    if (self->priv->status == AUDIO_STATUS_PAUSED) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: resuming playback (0x%X->0x%X)...\n", __debug__);
        if (!cdemud_audio_start_playing(self, error)) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: failed to start playback!\n", __debug__);
            succeeded = FALSE;
            goto end;
        }
    } else {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: resume called when not paused!\n", __debug__);
        cdemud_error(CDEMUD_E_AUDIOINVALIDSTATE, error);
        succeeded = FALSE;
        goto end;
    }
    
end:
    /* Unlock */
   // g_static_mutex_unlock(&self->priv->mutex);
    
    return succeeded;
}

gboolean cdemud_audio_pause (CDEMUD_Audio *self, GError **error)
{
    gboolean succeeded = TRUE;
    
    /* Pause is valid only if we are playing */
    if (self->priv->status == AUDIO_STATUS_PLAYING) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: pausing playback...\n", __debug__);
        
        if (!cdemud_audio_stop_playing(self, AUDIO_STATUS_PAUSED, error)) {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: failed to pause playback!\n", __debug__);
            succeeded = FALSE;
            goto end;
        }
    } else {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: pause called when not playing!\n", __debug__);
        cdemud_error(CDEMUD_E_AUDIOINVALIDSTATE, error);
        succeeded = FALSE;
        goto end;
    }
    
end:    
    return succeeded;
}

gboolean cdemud_audio_stop (CDEMUD_Audio *self, GError **error)
{
    gboolean succeeded = TRUE;
    
    /* Stop is valid only if we are playing or paused */
    if (self->priv->status == AUDIO_STATUS_PLAYING || self->priv->status == AUDIO_STATUS_PAUSED) {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: stopping playback...\n", __debug__);
        
        if (cdemud_audio_stop_playing(self, AUDIO_STATUS_NOSTATUS, error)) {
            /* Release disc reference */
            g_object_unref(self->priv->disc);
            self->priv->disc = NULL;
        } else {
            CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: failed to stop playback!\n", __debug__);
            succeeded = FALSE;
            goto end;
        }
    } else {
        CDEMUD_DEBUG(self, DAEMON_DEBUG_AUDIOPLAY, "%s: stop called when not playing nor paused!\n", __debug__);
        cdemud_error(CDEMUD_E_AUDIOINVALIDSTATE, error);
        succeeded = FALSE;
        goto end;
    }

end:
    return succeeded;
}

gboolean cdemud_audio_get_status (CDEMUD_Audio *self, gint *status, GError **error G_GNUC_UNUSED)
{    
    /* Return status */
    *status = self->priv->status;
    
    return TRUE;
}


/**********************************************************************\
 *                             Object init                            * 
\**********************************************************************/
G_DEFINE_TYPE(CDEMUD_Audio, cdemud_audio, MIRAGE_TYPE_OBJECT);

static void cdemud_audio_finalize (GObject *gobject)
{
    CDEMUD_Audio *self = CDEMUD_AUDIO(gobject);                                                
        
    /* Force the playback to stop */
    cdemud_audio_stop(self, NULL);
    
    /* Chain up to the parent class */
    return G_OBJECT_CLASS(cdemud_audio_parent_class)->finalize(gobject);
}

static void cdemud_audio_class_init (CDEMUD_AudioClass *klass)
{    
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    
    gobject_class->finalize = cdemud_audio_finalize;

    /* Register private structure */
    g_type_class_add_private(klass, sizeof(CDEMUD_AudioPrivate));
}

static void cdemud_audio_init (CDEMUD_Audio *self)
{
    self->priv = CDEMUD_AUDIO_GET_PRIVATE(self);
}
