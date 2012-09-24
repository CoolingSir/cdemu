/*
 *  CDEmu daemon: Debugging facilities
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

#ifndef __CDEMU_DEBUG_H__
#define __CDEMU_DEBUG_H__

/* Debug masks */
typedef enum
{
    /* Debug types; need to be same as in libMirage because we use its debug context */
    DAEMON_DEBUG_ERROR = MIRAGE_DEBUG_ERROR,
    DAEMON_DEBUG_WARNING = MIRAGE_DEBUG_WARNING,
    /* Debug masks */
    DAEMON_DEBUG_DEVICE = 0x0001,
    DAEMON_DEBUG_MMC = 0x0002,
    DAEMON_DEBUG_DELAY = 0x0004,
    DAEMON_DEBUG_AUDIOPLAY = 0x0008,
    DAEMON_DEBUG_KERNEL_IO = 0x0010,
} CdemuDeviceDebugMasks;

/* Debug macro */
#define CDEMU_DEBUG(obj, lvl, ...) MIRAGE_DEBUG(obj, lvl, __VA_ARGS__)

#endif /* __CDEMU_DEBUG_H__ */
