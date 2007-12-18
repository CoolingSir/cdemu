/*
 *  libMirage: SNDFILE fragment
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
 
#ifndef __MIRAGE_FRAGMENT_SNDFILE_H__
#define __MIRAGE_FRAGMENT_SNDFILE_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mirage.h"
#include <sndfile.h>
#include "fragment-sndfile-fragment.h"

G_BEGIN_DECLS

#define SNDFILE_FRAMES_PER_SECTOR (98*6)

GTypeModule *global_module;

G_END_DECLS

#endif /* __MIRAGE_FRAGMENT_SNDFILE_H__ */
