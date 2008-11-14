/*
 *  libMirage: B6T image parser: Parser object
 *  Copyright (C) 2007-2008 Rok Mandeljc
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

#include "image-b6t.h"

/* Self-explanatory */
#define WHINE_ON_UNEXPECTED(field, expected) \
    if (field != expected) MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: unexpected value in field %s: expected 0x%X, got 0x%X\n", __func__, #field, expected, field);


/******************************************************************************\
 *                              Private structure                             *
\******************************************************************************/
#define MIRAGE_PARSER_B6T_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), MIRAGE_TYPE_PARSER_B6T, MIRAGE_Parser_B6TPrivate))

typedef struct {
    GObject *disc;
    
    gchar *b6t_filename;
    
    GMappedFile *b6t_mapped;
    guint8 *b6t_data;
    guint8 *cur_ptr;
        
    B6T_DiscBlock_1 *disc_block_1;
    B6T_DiscBlock_2 *disc_block_2;
    guint8 *cdtext_data;
    
    gint32 prev_session_end;
    
    GList *data_blocks_list;
} MIRAGE_Parser_B6TPrivate;

static gboolean __mirage_parser_b6t_load_bwa_file (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    gboolean succeeded = TRUE;
    
    gchar *bwa_filename;
    gchar *bwa_fullpath;
    
    /* Use B6T filename and replace its extension with BWA */
    bwa_filename = g_strdup(_priv->b6t_filename);
    gint len = strlen(bwa_filename);
    sprintf(bwa_filename+len-3, "bwa");
    
    bwa_fullpath = mirage_helper_find_data_file(bwa_filename, _priv->b6t_filename);
    
    if (bwa_fullpath) {
        GMappedFile *bwa_mapped;
        GError *local_error = NULL;
        
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: found BWA file: '%s'\n", __func__, bwa_fullpath);
        
        /* Map BWA file */
        bwa_mapped = g_mapped_file_new(bwa_fullpath, FALSE, &local_error);
        
        if (bwa_mapped) {
            guint8 *cur_ptr = (guint8 *)g_mapped_file_get_contents(bwa_mapped);
            
            guint32 __dummy1__;
            guint32 __dummy2__;
            guint32 __dummy3__;
            
            guint32 dpm_start_sector;
            guint32 dpm_resolution;
            guint32 dpm_num_entries;
            
            guint32 *dpm_data;
            
            /* First three fields seem to have fixed values; probably some sort of
               header? */
            __dummy1__ = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
            cur_ptr += sizeof(guint32);
            
            __dummy2__ = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
            cur_ptr += sizeof(guint32);
            
            __dummy3__ = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
            cur_ptr += sizeof(guint32);
            
            WHINE_ON_UNEXPECTED(__dummy1__, 0x00000001);
            WHINE_ON_UNEXPECTED(__dummy2__, 0x00000008);
            WHINE_ON_UNEXPECTED(__dummy3__, 0x00000001);
            
            /* The next three fields are start sector, resolution and number
               of entries */
            dpm_start_sector = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
            cur_ptr += sizeof(guint32);
            
            dpm_resolution = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
            cur_ptr += sizeof(guint32);
            
            dpm_num_entries = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
            cur_ptr += sizeof(guint32);
            
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Start sector: 0x%X\n", __func__, dpm_start_sector);
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Resolution: %d\n", __func__, dpm_resolution);
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Number of entries: %d\n", __func__, dpm_num_entries);
            
            /* The rest is DPM data */
            dpm_data = MIRAGE_CAST_PTR(cur_ptr, 0, guint32 *);
            
            /* Set DPM data */
            if (!mirage_disc_set_dpm_data(MIRAGE_DISC(_priv->disc), dpm_start_sector, dpm_resolution, dpm_num_entries, dpm_data, error)) {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to set DPM data!\n", __func__);
                succeeded = FALSE;
            }
            
            g_mapped_file_free(bwa_mapped);
        } else {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to map file '%s': %s!\n", __func__, bwa_fullpath, local_error->message);
            g_error_free(local_error);
            mirage_error(MIRAGE_E_IMAGEFILE, error);
            succeeded = FALSE;
        }
    } else {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: no accompanying BWA file found\n", __func__);
    }
    
    g_free(bwa_fullpath);
    g_free(bwa_filename);
    
    return succeeded;
}

static gboolean __mirage_parser_b6t_parse_internal_dpm_data (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    
    if (_priv->disc_block_2->dpm_data_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: reading internal DPM data; 0x%X bytes\n", __func__, _priv->disc_block_2->dpm_data_length);
        
        guint8 *cur_ptr = _priv->cur_ptr;
        
        /* FIXME: by the look of it, this should actually support multiple blocks? */
        guint32 __dummy1__;
        guint32 __dummy2__;
        guint32 __dummy3__;
        guint32 __dummy4__;
        guint32 block_len1;
        guint32 block_len2;
        guint32 __dummy5__;
        guint32 __dummy6__;
        
        guint32 dpm_start_sector;
        guint32 dpm_resolution;
        guint32 dpm_num_entries;
            
        guint32 *dpm_data = NULL;
        
        /* Four fields that seem to have fixed values */
        __dummy1__ = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
        __dummy2__ = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
        __dummy3__ = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
        __dummy4__ = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
        
        WHINE_ON_UNEXPECTED(__dummy1__, 0x00000001);
        WHINE_ON_UNEXPECTED(__dummy2__, 0x00000001);
        WHINE_ON_UNEXPECTED(__dummy3__, 0x00000000);
        WHINE_ON_UNEXPECTED(__dummy4__, 0x00000000);
        
        /* Next two fields seem to have same value, which appears to be length
           of DPM data */
        block_len1 = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
        block_len2 = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: DPM block length: 0x%X, 0x%X (numbers should be same)\n", __func__, block_len1, block_len2);
        
        /* Two more undeciphered fields */
        __dummy5__ = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
        __dummy6__ = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
        
        WHINE_ON_UNEXPECTED(__dummy5__, 0x00000000);
        WHINE_ON_UNEXPECTED(__dummy6__, 0x00000001);
        
        /* The next three fields are start sector, resolution and number
            of entries */
        dpm_start_sector = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
            
        dpm_resolution = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
            
        dpm_num_entries = MIRAGE_CAST_DATA(cur_ptr, 0, guint32);
        cur_ptr += sizeof(guint32);
            
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Start sector: 0x%X\n", __func__, dpm_start_sector);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Resolution: %d\n", __func__, dpm_resolution);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Number of entries: %d\n", __func__, dpm_num_entries);
            
        /* The rest is DPM data */
        dpm_data = MIRAGE_CAST_PTR(cur_ptr, 0, guint32 *);
        cur_ptr += dpm_num_entries * sizeof(guint32);
        
        /* Set DPM data */
        if (!mirage_disc_set_dpm_data(MIRAGE_DISC(_priv->disc), dpm_start_sector, dpm_resolution, dpm_num_entries, dpm_data, error)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to set DPM data!\n", __func__);
            return FALSE;
        }
        
        /* Calculate length of data we've processed */
        gsize length = (gsize)cur_ptr - (gsize)_priv->cur_ptr;
        if (length != _priv->disc_block_2->dpm_data_length) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: I'm afraid Dave... we read 0x%zX bytes, declared size is 0x%X bytes\n", __func__, length, _priv->disc_block_2->dpm_data_length);
        }
        
        /* Skip the whole block, so that parsing errors here aren't fatal */
        _priv->cur_ptr += _priv->disc_block_2->dpm_data_length;
    }
    
    return TRUE;
}

static gboolean __mirage_parser_b6t_setup_track_fragments (MIRAGE_Parser *self, GObject *cur_track, gint start_sector, gint length, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    GList *entry;
    
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: setting up data blocks for track starting at sector 0x%X (%i), length 0x%X\n", __func__, start_sector, start_sector, length);
    
    /* Data blocks are nice concept that's similar to libMirage's fragments; they
       are chunks of continuous data, stored in the same file and read in the same
       mode. To illustrate how it goes, consider a parser with single Mode 1 data track
       and three Audio tracks - data track will be stored in one data block, and the
       three audio tracks in another, but both data blocks will be stored in the same
       file, just at different offsets. Note that since they are separated, one data
       block could include subchannel and the other not. On the other hand, consider
       now a couple-of-GB DVD image. It will get split into 2 GB files, each being
       its own block. But all the blocks will make up a single track. So contrary to
       libMirage's fragments, where fragments are subunits of tracks, it is possible
       for single data block to contain data for multiple tracks. */
    G_LIST_FOR_EACH(entry, _priv->data_blocks_list) {
        B6T_DataBlock *data_block = entry->data;
        
        if (start_sector >= data_block->start_sector && start_sector < data_block->start_sector + data_block->length_sectors) {
            gint tmp_length;
            gchar *filename;
            GObject *data_fragment;
            
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: found a block %i\n", __func__, g_list_position(_priv->data_blocks_list, entry));
            
            tmp_length = MIN(length, data_block->length_sectors);
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: using length: 0x%X\n", __func__, tmp_length);
                        
            /* Find filename */
            filename = mirage_helper_find_data_file(data_block->filename, _priv->b6t_filename);
            if (!filename) {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to find data file '%s'\n", __func__, data_block->filename);
                mirage_error(MIRAGE_E_DATAFILE, error);
                return FALSE;
            }
            
            /* We'd like a BINARY fragment */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: creating BINARY fragment\n", __func__);
            data_fragment = libmirage_create_fragment(MIRAGE_TYPE_FINTERFACE_BINARY, filename, error);
            if (!data_fragment) {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to create BINARY fragment!\n", __func__);
                return FALSE;
            }
                    
            FILE *tfile_handle = NULL;
            gint tfile_sectsize = 0;
            gint tfile_format = 0;
            guint64 tfile_offset = 0;
            
            gint sfile_format = 0;
            gint sfile_sectsize = 0;
                    
            /* Track file */
            tfile_handle = g_fopen(filename, "r");
            g_free(filename);
            /* We calculate sector size... */
            tfile_sectsize = data_block->length_bytes/data_block->length_sectors;
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: track file sector size: %i (0x%X)\n", __func__, tfile_sectsize, tfile_sectsize);
            /* Use sector size to calculate offset */
            tfile_offset = data_block->offset + (start_sector - data_block->start_sector)*tfile_sectsize;
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: track file offset: 0x%llX\n", __func__, tfile_offset);
            /* Adjust sector size to account for subchannel */
            if (tfile_sectsize > 2352) {
                /* If it's more than full sector, we have subchannel */
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: track file sector size implies subchannel data...\n", __func__);
                sfile_sectsize = tfile_sectsize - 2352;
                tfile_sectsize = 2352;
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: subchannel sector size: %i (0x%X)\n", __func__, sfile_sectsize, sfile_sectsize);
                switch (sfile_sectsize) {
                    case 16: {
                        /* Internal subchannel, PQ */
                        sfile_format = FR_BIN_SFILE_PQ16 | FR_BIN_SFILE_INT;                        
                        break;
                    }
                    case 96: {
                        /* Internal subchannel, linear PW96 */
                        sfile_format = FR_BIN_SFILE_PW96_LIN | FR_BIN_SFILE_INT;
                        break;
                    }
                    default: {
                        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: unhandled subchannel sector size: %i (0x%X)\n", __func__, sfile_sectsize, sfile_sectsize);
                        break;
                    }
                }
            }
            
            /* Data format: */
            if ((data_block->type & 0x00008000) == 0x00008000) {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: data block is for audio data\n", __func__);
                tfile_format = FR_BIN_TFILE_AUDIO;
            } else {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: data block is for data track\n", __func__);
                tfile_format = FR_BIN_TFILE_DATA;
            }
            
            mirage_finterface_binary_track_file_set_handle(MIRAGE_FINTERFACE_BINARY(data_fragment), tfile_handle, NULL);
            mirage_finterface_binary_track_file_set_sectsize(MIRAGE_FINTERFACE_BINARY(data_fragment), tfile_sectsize, NULL);
            mirage_finterface_binary_track_file_set_offset(MIRAGE_FINTERFACE_BINARY(data_fragment), tfile_offset, NULL);
            mirage_finterface_binary_track_file_set_format(MIRAGE_FINTERFACE_BINARY(data_fragment), tfile_format, NULL);

            mirage_finterface_binary_subchannel_file_set_sectsize(MIRAGE_FINTERFACE_BINARY(data_fragment), sfile_sectsize, NULL);
            mirage_finterface_binary_subchannel_file_set_format(MIRAGE_FINTERFACE_BINARY(data_fragment), sfile_format, NULL);
             
            mirage_fragment_set_length(MIRAGE_FRAGMENT(data_fragment), tmp_length, NULL);
            
            /* Add fragment */
            mirage_track_add_fragment(MIRAGE_TRACK(cur_track), -1, &data_fragment, NULL);
            g_object_unref(data_fragment);
            
            
            /* Calculate remaining track length */
            length -= tmp_length;
            
            if (length == 0) {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: used up all length, breaking\n", __func__);
                break;
            } else {
                /* Calculate start sector for the remainder */
                start_sector += tmp_length;
            }
        }
    }
    
    return TRUE;
}

static gboolean __mirage_parser_b6t_parse_header (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    gchar *header;
    
    /* Read header (16 bytes) */
    header = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, gchar *);
    _priv->cur_ptr += 16;
    
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: file header: %.16s\n", __func__, header);
    
    /* Make sure it's correct one */
    if (memcmp(header, "BWT5 STREAM SIGN", 16)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: invalid header!\n", __func__);
        mirage_error(MIRAGE_E_PARSER, error);
        return FALSE;
    }
    
    return TRUE;
}


static gboolean __mirage_parser_b6t_parse_pma_data (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    
    if (_priv->disc_block_1->pma_data_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: PMA data not used yet; skipping 0x%X bytes\n", __func__, _priv->disc_block_1->pma_data_length);
        _priv->cur_ptr += _priv->disc_block_1->pma_data_length;
    }
    
    return TRUE;
}

static gboolean __mirage_parser_b6t_parse_atip_data (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    
    if (_priv->disc_block_1->atip_data_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: ATIP data not used yet; skipping 0x%X bytes\n", __func__, _priv->disc_block_1->atip_data_length);
        _priv->cur_ptr += _priv->disc_block_1->atip_data_length;
    }
    
    return TRUE;
}

static gboolean __mirage_parser_b6t_parse_cdtext_data (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    
    if (_priv->disc_block_1->cdtext_data_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: reading CD-TEXT data; 0x%X bytes\n", __func__, _priv->disc_block_1->cdtext_data_length);
        /* Read; we don't set data here, because at this point we don't have
           parser layout set up yet */
        _priv->cdtext_data = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, guint8 *);
        _priv->cur_ptr += _priv->disc_block_1->cdtext_data_length;
    }
    
    return TRUE;
}

static gboolean __mirage_parser_b6t_parse_bca (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    
    if (_priv->disc_block_1->dvdrom_bca_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: reading BCA data; 0x%X bytes\n", __func__, _priv->disc_block_1->dvdrom_bca_length);
        
        /* Read */
        guint8 *bca_data = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, guint8 *);
        _priv->cur_ptr += _priv->disc_block_1->dvdrom_bca_length;
        
        if (!mirage_disc_set_disc_structure(MIRAGE_DISC(_priv->disc), 0, 0x0003, bca_data, _priv->disc_block_1->dvdrom_bca_length, error)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to set BCA data!\n", __func__);
        }
    }
    
    return TRUE;
}

static gboolean __mirage_parser_b6t_parse_dvd_structures (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    gint length = 0;
    
    /* Return if there's nothing to do */
    if (!_priv->disc_block_1->dvdrom_structures_length) {
        return TRUE;
    }
    
    /* Hmm... it seems there are two bytes set to 0 preceeding the structures */
    guint16 dummy = MIRAGE_CAST_DATA(_priv->cur_ptr, 0, guint16);
    _priv->cur_ptr += sizeof(guint16);
    WHINE_ON_UNEXPECTED(dummy, 0);
    length = sizeof(dummy);
    
    while (length < _priv->disc_block_1->dvdrom_structures_length) {
        /* It seems DVD structures are stored in following format:
             - 2 bytes, holding structure number
             - 4 bytes, holding data header as returned by READ PARSER STRUCTURE
               command (i.e. 2 bytes of data length and 2 reserved bytes)
             - actual data
        */
        guint16 struct_number;
        guint16 struct_length;
        guint16 struct_reserved;
        guint8 *struct_data;
        
        /* Read structure number and data length */
        struct_number = MIRAGE_CAST_DATA(_priv->cur_ptr, 0, guint16);
        _priv->cur_ptr += sizeof(guint16);
        
        struct_length = MIRAGE_CAST_DATA(_priv->cur_ptr, 0, guint16);
        _priv->cur_ptr += sizeof(guint16);
        
        struct_reserved = MIRAGE_CAST_DATA(_priv->cur_ptr, 0, guint16);
        _priv->cur_ptr += sizeof(guint16);
        
        /* Length in header is big-endian; and it also includes two reserved 
           bytes following it, so make sure this is accounted for */
        struct_length = GUINT16_FROM_BE(struct_length) - 2;
        WHINE_ON_UNEXPECTED(struct_reserved, 0x0000);
        
        /* Allocate buffer and read data */
        struct_data = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, guint8 *);
        _priv->cur_ptr += struct_length;
        
        /* Set structure */
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: structure 0x%X, length: 0x%X\n", __func__, struct_number, struct_length);
        if (!mirage_disc_set_disc_structure(MIRAGE_DISC(_priv->disc), 0, struct_number, struct_data, struct_length, error)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to set structure data for structure 0x%X!\n", __func__, struct_number);
        }
                
        length += sizeof(struct_number) + sizeof(struct_length) + sizeof(struct_reserved) + struct_length;
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: read %d out of %d bytes...\n", __func__, length, _priv->disc_block_1->dvdrom_structures_length);
    }
    
    return TRUE;
}


static gboolean __mirage_parser_b6t_decode_parser_type (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);

    switch (_priv->disc_block_1->disc_type) {
        case 0x08: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: CD-ROM parser\n", __func__);
            mirage_disc_set_medium_type(MIRAGE_DISC(_priv->disc), MIRAGE_MEDIUM_CD, NULL);
            break;
        }
        case 0x10: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: DVD-ROM parser\n", __func__);
            mirage_disc_set_medium_type(MIRAGE_DISC(_priv->disc), MIRAGE_MEDIUM_DVD, NULL);
            break;
        }
        case 0x11: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: DVD-R parser\n", __func__);
            mirage_disc_set_medium_type(MIRAGE_DISC(_priv->disc), MIRAGE_MEDIUM_DVD, NULL);
            break;
        }
        default: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: unknown parser type: 0x%X!\n", __func__, _priv->disc_block_1->disc_type);
            mirage_error(MIRAGE_E_PARSER, error);
            return FALSE;
        }
    }
    
    return TRUE;
}

static gboolean __mirage_parser_b6t_parse_disc_blocks (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    
    /* 112 bytes a.k.a. first parser block */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: reading 'disc block 1'\n", __func__);
    _priv->disc_block_1 = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, B6T_DiscBlock_1 *);
    _priv->cur_ptr += sizeof(B6T_DiscBlock_1);
    
    /* Since most of these fields are not deciphered yet, watch out for 
       deviations from 'usual' values */
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy1__, 0x00000002);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy2__, 0x00000002);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy3__, 0x00000006);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy4__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy5__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy6__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy7__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy8__, 0x00000000);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  disc type: 0x%X\n", __func__, _priv->disc_block_1->disc_type);
    __mirage_parser_b6t_decode_parser_type(self, error);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  number of sessions: %i\n", __func__, _priv->disc_block_1->num_sessions);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy9__, 0x00000002);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy10__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy11__, 0x00000000);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  MCN valid: 0x%X\n", __func__, _priv->disc_block_1->mcn_valid);
    if (_priv->disc_block_1->mcn_valid) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  MCN: %.13s\n", __func__, _priv->disc_block_1->mcn);
    }
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy12__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy13__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy14__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy15__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy16__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy17__, 0x00000000);    
    if (_priv->disc_block_1->disc_type == 0x08) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  PMA data length: 0x%X\n", __func__, _priv->disc_block_1->pma_data_length);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  ATIP data length: 0x%X\n", __func__, _priv->disc_block_1->atip_data_length);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  CD-TEXT data length: 0x%X\n", __func__, _priv->disc_block_1->cdtext_data_length);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  CD-ROM info block length: 0x%X\n", __func__, _priv->disc_block_1->cdrom_info_length);
    } else {
        WHINE_ON_UNEXPECTED(_priv->disc_block_1->pma_data_length, 0x0000);
        WHINE_ON_UNEXPECTED(_priv->disc_block_1->atip_data_length, 0x0000);
        WHINE_ON_UNEXPECTED(_priv->disc_block_1->cdtext_data_length, 0x0000);
        WHINE_ON_UNEXPECTED(_priv->disc_block_1->cdrom_info_length, 0x0000);
    }
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy19__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy20__, 0x00000000);
    WHINE_ON_UNEXPECTED(_priv->disc_block_1->__dummy21__, 0x00000000);
    if (_priv->disc_block_1->disc_type == 0x10) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  BCA length: 0x%X\n", __func__, _priv->disc_block_1->dvdrom_bca_length);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  parser structures length: 0x%X\n", __func__, _priv->disc_block_1->dvdrom_structures_length);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  DVD-ROM info block length: 0x%X\n", __func__, _priv->disc_block_1->dvdrom_info_length);
    } else {
        WHINE_ON_UNEXPECTED(_priv->disc_block_1->dvdrom_bca_length, 0x00000000);
        WHINE_ON_UNEXPECTED(_priv->disc_block_1->dvdrom_structures_length, 0x00000000);
        WHINE_ON_UNEXPECTED(_priv->disc_block_1->dvdrom_info_length, 0x00000000);
    }
    
    
    /* 32 junk bytes; these seem to have some meaning for non-audio CDs... */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: skipping 32 undeciphered bytes\n", __func__);
    _priv->cur_ptr += 32;
    
    
    /* Next 28 bytes are drive identifiers; these are part of data returned by 
       INQUIRY command */
    B6T_DriveIdentifiers *inquiry_id = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, B6T_DriveIdentifiers *);
    _priv->cur_ptr += sizeof(B6T_DriveIdentifiers);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: image was created with following drive:\n", __func__);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  vendor: %.8s\n", __func__, inquiry_id->vendor);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  product: %.16s\n", __func__, inquiry_id->product);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  revision: %.4s\n", __func__, inquiry_id->revision);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  vendor specific: %.20s\n", __func__, inquiry_id->vendor_specific);

    
    /* Then there's 32 bytes of ISO volume descriptor; they represent volume ID,
       if it is a data CD, or they're set to AUDIO CD in case of audio CD */
    gchar *volume_id = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, gchar *);
    _priv->cur_ptr += 32;
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: volume ID: %.32s\n", __func__, volume_id);
    
    
    /* What comes next is 20 bytes that are seemingly organised into 32-bit 
       integers... experimenting with different layouts show that these are
       indeed lengths of blocks that follow */
    _priv->disc_block_2 = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, B6T_DiscBlock_2 *);
    _priv->cur_ptr += sizeof(B6T_DiscBlock_2);
        
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: reading 'disc block 2'\n", __func__);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  mode page 0x2A data length: 0x%X\n", __func__, _priv->disc_block_2->mode_page_2a_length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  unknown block 1 data length: 0x%X\n", __func__, _priv->disc_block_2->unknown1_length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  data-blocks data length: 0x%X\n", __func__, _priv->disc_block_2->datablocks_length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  sessions data length: 0x%X\n", __func__, _priv->disc_block_2->sessions_length);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  DPM data length: 0x%X\n", __func__, _priv->disc_block_2->dpm_data_length);   
    
    
    /* Right, you thought so far everything was pretty much straightforward? Well,
       fear not... It would seem that data to follow are blocks whose lengths were 
       defined in both disc_block_1 and disc_block_2 - and to make things worse, 
       they're mixed... */

    
    /* Mode Page 0x2A: CD/DVD Capabilities and Mechanical Status Mode Page...
       this one is returned in response to MODE SENSE (10) that Blindwrite issues;
       note that the page per-se doesn't have any influence on image data, and it's
       probably included just for diagnostics or somesuch. Therefore we won't be
       reading it (maybe later, just to dump the data...) */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: skipping Mode Page 0x2A (0x%X bytes)\n", __func__, _priv->disc_block_2->mode_page_2a_length);
    _priv->cur_ptr += _priv->disc_block_2->mode_page_2a_length;
    
    /* Unknown data block #1... this one seems to be 4 bytes long in all images
       I've tested with, and set to 0. For now, skip it, but print a warning if
       it's not 4 bytes long */
    WHINE_ON_UNEXPECTED(_priv->disc_block_2->unknown1_length, 4);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: skipping Unknown data block #1 (0x%X bytes)\n", __func__, _priv->disc_block_2->unknown1_length);
    _priv->cur_ptr += _priv->disc_block_2->unknown1_length;
            
    /* This is where PMA/ATIP/CD-TEXT data gets stored, in that order */
    if (!__mirage_parser_b6t_parse_pma_data(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse PMA data!\n", __func__);
        return FALSE;
    }
    if (!__mirage_parser_b6t_parse_atip_data(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse ATIP data!\n", __func__);
        return FALSE;
    }
    if (!__mirage_parser_b6t_parse_cdtext_data(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse CD-TEXT data!\n", __func__);
        return FALSE;
    }
    
    /* If we're dealing with DVD-ROM image, this is where BCA is stored, followed
       by parser structures... Since I don't think PMA/ATIP/CD-TEXT can be obtained
       from DVD-ROMs, it doesn't really matter which should come first (though, 
       judging by order of length integers, I'd say this is correct order...) */
    if (!__mirage_parser_b6t_parse_bca(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse BCA!\n", __func__);
        return FALSE;
    }
    if (!__mirage_parser_b6t_parse_dvd_structures(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse DVD structures!\n", __func__);
        return FALSE;
    }
    
    /* Next 56 bytes (0x38) seem to be the block whose length seems to be declared 
       in disc_block_1... The block is present in both CD-ROM and DVD-ROM images, but
       its length seems to be declared in different positions. I can't make much out 
       of the data itself, expect that the last 8 bytes are verbatim copy of data 
       returned by READ CAPACITY command. Again, this data is not really relevant, 
       so we're skipping it... */
    if (_priv->disc_block_1->disc_type == 0x08) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: skipping CD-ROM parser info block (0x%X bytes)\n", __func__, _priv->disc_block_1->cdrom_info_length);
        _priv->cur_ptr += _priv->disc_block_1->cdrom_info_length;
    } else {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: skipping DVD-ROM parser info block (0x%X bytes)\n", __func__, _priv->disc_block_1->dvdrom_info_length);
        _priv->cur_ptr += _priv->disc_block_1->dvdrom_info_length;
    }
    
    return TRUE;
}

static gint __sort_data_blocks (B6T_DataBlock *block1, B6T_DataBlock *block2) {
    /* Data blocks should be ordered according to their start sectors */
    if (block1->start_sector < block2->start_sector) {
        return -1;
    } else if (block1->start_sector > block2->start_sector) {
        return 1;
    } else {
        return 0;
    }
}

static gboolean __mirage_parser_b6t_parse_data_blocks (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    
    gsize length = 0;
    gint i;
    
    /* Store the current pointer (for length calculation) */
    length = (gsize)_priv->cur_ptr;
        
    /* First four bytes are number of data blocks */
    guint32 num_data_blocks = MIRAGE_CAST_DATA(_priv->cur_ptr, 0, guint32);
    _priv->cur_ptr += sizeof(guint32);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: number of data blocks: %i\n", __func__, num_data_blocks);
    
    /* Then there's drive path; it seems it's awfully important to B6T image which
       drive it's been created on... it's irrelevant to us, so skip it */
    guint32 drive_path_length = MIRAGE_CAST_DATA(_priv->cur_ptr, 0, guint32);
    _priv->cur_ptr += sizeof(guint32);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: skipping 0x%X bytes of drive path\n", __func__, drive_path_length);    
    _priv->cur_ptr += drive_path_length;
    
    /* Now, the actual blocks; we need to copy these, because filename field 
       needs to be changed */
    for (i = 0; i < num_data_blocks; i++) {
        B6T_DataBlock *data_block = g_new0(B6T_DataBlock, 1);
        
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: data block #%i\n", __func__, i);
        /* Read data block up to the filename */
        memcpy(data_block, MIRAGE_CAST_PTR(_priv->cur_ptr, 0, B6T_DataBlock *), offsetof(B6T_DataBlock, filename));
        _priv->cur_ptr += offsetof(B6T_DataBlock, filename);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  type: 0x%X\n", __func__, data_block->type);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  length (bytes): 0x%X\n", __func__, data_block->length_bytes);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  offset: 0x%X\n", __func__, data_block->offset);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  start_sector: 0x%X\n", __func__, data_block->start_sector);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  length (sectors): 0x%X\n", __func__, data_block->length_sectors);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  filename_length: %i\n", __func__, data_block->filename_length);
        /* Temporary UTF-16 filename... note that filename_length is actual 
           length in bytes, not characters! */
        gunichar2 *tmp_filename = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, gunichar2 *);
        _priv->cur_ptr += data_block->filename_length;
        /* Convert filename */
        data_block->filename = g_utf16_to_utf8(tmp_filename, data_block->filename_length/2, NULL, NULL, NULL);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  filename: %s\n", __func__, data_block->filename);        
        /* Read the trailing four bytes */
        data_block->__dummy8__ = MIRAGE_CAST_DATA(_priv->cur_ptr, 0, guint32);
        _priv->cur_ptr += sizeof(guint32);
        
        /* Add block to the list */
        _priv->data_blocks_list = g_list_insert_sorted(_priv->data_blocks_list, data_block, (GCompareFunc)__sort_data_blocks);
    }
        
    /* Calculate length of data we've processed */
    length = (gsize)_priv->cur_ptr - length;
    
    if (length != _priv->disc_block_2->datablocks_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: I'm afraid Dave... we read 0x%zX bytes, declared size is 0x%X bytes (%d data blocks)\n", __func__, length, _priv->disc_block_2->datablocks_length, num_data_blocks);
    }
    
    return TRUE;
}

static gboolean __mirage_parser_b6t_parse_track_entry (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    GObject *cur_track = NULL;
    B6T_Track *track = NULL;
    
    track = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, B6T_Track *);
    _priv->cur_ptr += sizeof(B6T_Track);
        
    /* We have no use for non-track descriptors at the moment */
    if (track->type == 0) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: skipping non-track descriptor, point 0x%X\n", __func__, track->point);
        return TRUE;
    }

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: loading track descriptor:\n", __func__);    
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   type: 0x%X\n", __func__, track->type);
    if (track->type == 1 || track->type == 6) {
        /* 0 for Audio and DVD tracks */
        WHINE_ON_UNEXPECTED(track->__dummy1__, 0x00);
        WHINE_ON_UNEXPECTED(track->__dummy2__, 0x00);
        WHINE_ON_UNEXPECTED(track->__dummy3__, 0x00);
        WHINE_ON_UNEXPECTED(track->__dummy4__, 0x00000000);
    } else {
        WHINE_ON_UNEXPECTED(track->__dummy1__, 0x01);
        WHINE_ON_UNEXPECTED(track->__dummy2__, 0x01);
        WHINE_ON_UNEXPECTED(track->__dummy3__, 0x01);
        WHINE_ON_UNEXPECTED(track->__dummy4__, 0x00000001);
    }
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   subchannel: 0x%X\n", __func__, track->subchannel);
    WHINE_ON_UNEXPECTED(track->__dummy5__, 0x00);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   CTL: 0x%X\n", __func__, track->ctl);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   ADR: 0x%X\n", __func__, track->adr);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   point: 0x%X\n", __func__, track->point);
    WHINE_ON_UNEXPECTED(track->__dummy6__, 0x00);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   MSF: %02i:%02i:%02i\n", __func__, track->min, track->sec, track->frame);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   zero: %i\n", __func__, track->zero);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   PMSF: %02i:%02i:%02i\n", __func__, track->pmin, track->psec, track->pframe);
    if (track->type == 6) {
        /* 0 for DVD tracks */
        WHINE_ON_UNEXPECTED(track->__dummy7__, 0x00);
    } else {
        WHINE_ON_UNEXPECTED(track->__dummy7__, 0x01);
    }
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   pregap length: 0x%X\n", __func__, track->pregap);
    WHINE_ON_UNEXPECTED(track->__dummy8__, 0x00000000);
    WHINE_ON_UNEXPECTED(track->__dummy9__, 0x00000000);
    WHINE_ON_UNEXPECTED(track->__dummy10__, 0x00000000);
    WHINE_ON_UNEXPECTED(track->__dummy11__, 0x00000000);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   start sector: 0x%X\n", __func__, track->start_sector);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   track length: 0x%X\n", __func__, track->length);
    WHINE_ON_UNEXPECTED(track->__dummy12__, 0x00000000);
    WHINE_ON_UNEXPECTED(track->__dummy13__, 0x00000000);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   session number: 0x%X\n", __func__, track->session_number);    
    WHINE_ON_UNEXPECTED(track->__dummy14__, 0x0000);
    
    /* It seems only non-DVD track entries have additional 8 bytes */
    if (track->type != 6 && track->type != 0) {
        _priv->cur_ptr += 8;
    }
    
    /* Create track now; we'll add it directly to disc, with disc and track number we have */
    if (!mirage_disc_add_track_by_number(MIRAGE_DISC(_priv->disc), track->point, &cur_track, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to add track!\n", __func__);
        return FALSE;
    }
    
    /* Track mode... Seems type field for track descriptor determines that:
        - 0: non-track descriptor
        - 1: Audio track
        - 2: Mode 1 track
        - 3: Mode 2 Formless track
        - 4: Mode 2 Form 1 track
        - 5: Mode 2 Form 2 track
        - 6: DVD track
    */
    switch (track->type) {
        case 1: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Audio track\n", __func__);
            mirage_track_set_mode(MIRAGE_TRACK(cur_track), MIRAGE_MODE_AUDIO, NULL);
            break;
        }
        case 2: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 1 track\n", __func__);
            mirage_track_set_mode(MIRAGE_TRACK(cur_track), MIRAGE_MODE_MODE1, NULL);
            break;
        }
        case 3: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 2 track\n", __func__);
            mirage_track_set_mode(MIRAGE_TRACK(cur_track), MIRAGE_MODE_MODE2_MIXED, NULL);
            break;
        }
        case 4: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 2 Form 1 track\n", __func__);
            mirage_track_set_mode(MIRAGE_TRACK(cur_track), MIRAGE_MODE_MODE2_FORM1, NULL);
            break;
        }
        case 5: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 2 Form 2 track\n", __func__);
            mirage_track_set_mode(MIRAGE_TRACK(cur_track), MIRAGE_MODE_MODE2_FORM2, NULL);
            break;
        }            
        case 6: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: DVD track\n", __func__);
            mirage_track_set_mode(MIRAGE_TRACK(cur_track), MIRAGE_MODE_MODE1, NULL);
            break;
        }
        default: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: unknown track type: 0x%X!\n", __func__, track->type);
            mirage_error(MIRAGE_E_PARSER, error);
            g_object_unref(cur_track);
            return FALSE;
        }
    }
    
    /* Set up track fragments */
    if (!__mirage_parser_b6t_setup_track_fragments(self, cur_track, track->start_sector, track->length, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to set up track's fragments!\n", __func__);
        g_object_unref(cur_track);
        return FALSE;
    }
    
    /* Set track start */
    mirage_track_set_track_start(MIRAGE_TRACK(cur_track), track->pregap, NULL);
    
    g_object_unref(cur_track);
    return TRUE;
}

static gboolean __mirage_parser_b6t_parse_session (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    gint i;
    B6T_Session *session;
    
    session = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, B6T_Session *);
    _priv->cur_ptr += sizeof(B6T_Session);
    
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: loading session:\n", __func__);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   number: %i\n", __func__, session->number);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   number of entries: %i\n", __func__, session->num_entries);
    WHINE_ON_UNEXPECTED(session->__dummy1__, 3);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   start address: 0x%X\n", __func__, session->session_start);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   end address: 0x%X\n", __func__, session->session_end);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   first track: 0x%X\n", __func__, session->first_track);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   end track: 0x%X\n", __func__, session->last_track);
    
    /* If this is the first session, its starting address is also the starting address
       of the parser... if not, we need to set the length of lead-out of previous session
       (which would equal difference between previous end and current start address) */
    if (session->number == 1) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: first session; setting parser's start to 0x%X (%i)\n", __func__, session->session_start, session->session_start);
        mirage_disc_layout_set_start_sector(MIRAGE_DISC(_priv->disc), session->session_start, NULL);
    } else {
        guint32 leadout_length = session->session_start - _priv->prev_session_end;
        GObject *prev_session = NULL;
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: previous session's leadout length: 0x%X (%i)\n", __func__, leadout_length, leadout_length);
        
        if (!mirage_disc_get_session_by_number(MIRAGE_DISC(_priv->disc), session->number - 1, &prev_session, error)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to get previous session!\n", __func__);
            return FALSE;
        }
        
        if (!mirage_session_set_leadout_length(MIRAGE_SESSION(prev_session), leadout_length, error)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to set leadout length!\n", __func__);
            g_object_unref(prev_session);
            return FALSE;
        }
        
        g_object_unref(prev_session);
    }
    _priv->prev_session_end = session->session_end;
    
    /* Add session */
    if (!mirage_disc_add_session_by_number(MIRAGE_DISC(_priv->disc), session->number, NULL, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to add session!\n", __func__);
        return FALSE;
    }
    
    /* Load track entries */
    for (i = 0; i < session->num_entries; i++) {
        if (!__mirage_parser_b6t_parse_track_entry(self, error)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse track entry #%i!\n", __func__, i);
            return FALSE;
        }
    }
    
    
    return TRUE;
}

static gboolean __mirage_parser_b6t_parse_sessions (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);   
    gsize length = 0;
    gint i;
    
    /* Store the offset */
    length = (gsize)_priv->cur_ptr;
    
    /* Load each session */
    for (i = 0; i < _priv->disc_block_1->num_sessions; i++) {
        if (!__mirage_parser_b6t_parse_session(self, error)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse session #%i!\n", __func__, i);
            return FALSE;
        }
    }
        
    /* Calculate length of data we've processed */
    length = (gsize)_priv->cur_ptr - length;
    
    if (length != _priv->disc_block_2->sessions_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: I'm afraid Dave... we read 0x%zX bytes, declared size is 0x%X bytes\n", __func__, length, _priv->disc_block_2->sessions_length);
    }  

    return TRUE;
}

static gboolean __mirage_parser_b6t_parse_footer (MIRAGE_Parser *self, GError **error) {
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    
    /* Read footer */
    gchar *footer = MIRAGE_CAST_PTR(_priv->cur_ptr, 0, gchar *);
    _priv->cur_ptr += 16;

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: file footer: %.16s\n", __func__, footer);
    
    /* Make sure it's correct one */
    if (memcmp(footer, "BWT5 STREAM FOOT", 16)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: invalid footer!\n", __func__);
        mirage_error(MIRAGE_E_PARSER, error);
        return FALSE;
    }
    
    return TRUE;
}

static gboolean __mirage_parser_b6t_load_parser (MIRAGE_Parser *self, GError **error) {   
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);

    /* Start at the beginning */
    _priv->cur_ptr = _priv->b6t_data;
    
    /* Read header */
    if (!__mirage_parser_b6t_parse_header(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse header!\n");
        return FALSE;
    }
    
    /* Read parser blocks */
    if (!__mirage_parser_b6t_parse_disc_blocks(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse parser blocks!\n");
        return FALSE;
    }
    
    /* Read data blocks */
    if (!__mirage_parser_b6t_parse_data_blocks(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse data blocks!\n");
        return FALSE;
    }
    
    /* Read sessions */
    if (!__mirage_parser_b6t_parse_sessions(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse sessions!\n");
        return FALSE;
    }
    
    /* Read internal DPM data */
    if (!__mirage_parser_b6t_parse_internal_dpm_data(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse internal DPM data!\n");
        return FALSE;
    }
    
    /* Read B6T file length */
    guint32 b6t_length = MIRAGE_CAST_DATA(_priv->cur_ptr, 0, guint32);
    _priv->cur_ptr += sizeof(guint32);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: declared B6T file length: %i (0x%X) bytes\n", __func__, b6t_length, b6t_length);
    

    /* Read footer */
    if (!__mirage_parser_b6t_parse_footer(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to parse footer!\n");
        return FALSE;
    }
    
    /* Try to load external BWA file; the internal DPM data does not necessarily
       contain entries for whole parser, but rather just for beginning of the parser
       (which is usually checked by copy protection). BWA file, on the other hand,
       usually contains DPM data for the whole parser. So, if we have both internal
       DPM data and a BWA available, we'll use the latter... */
    if (!__mirage_parser_b6t_load_bwa_file(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to load BWA file!\n");
        return FALSE;
    }
    
    return TRUE;    
}

/******************************************************************************\
 *                     MIRAGE_Parser methods implementation                     *
\******************************************************************************/
static gboolean __mirage_parser_b6t_load_image (MIRAGE_Parser *self, gchar **filenames, GObject **disc, GError **error) {   
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    GError *local_error = NULL;
    gboolean succeeded = TRUE;
    FILE *file;
    guint8 header[16];
    
    /* Check if we can load the image */
    file = g_fopen(filenames[0], "r");
    if (!file) {
        mirage_error(MIRAGE_E_IMAGEFILE, error);
        return FALSE;
    }
    
    /* Read and verify header; we could also check the footer, but I think
       header check only is sufficient */
    fseeko(file, 0, SEEK_SET);
    if (fread(header, 16, 1, file) < 1) {
        fclose(file);
        mirage_error(MIRAGE_E_READFAILED, error);
        return FALSE;
    }
    fclose(file);

    if (memcmp(header, "BWT5 STREAM SIGN", 16)) {
        mirage_error(MIRAGE_E_CANTHANDLE, error);
        return FALSE;
    }
    
    /* Create disc */
    _priv->disc = g_object_new(MIRAGE_TYPE_DISC, NULL);
    mirage_object_attach_child(MIRAGE_OBJECT(self), _priv->disc, NULL);

    mirage_disc_set_filename(MIRAGE_DISC(_priv->disc), filenames, NULL);
    _priv->b6t_filename = g_strdup(filenames[0]);
    
    /* Map the file using GLib's GMappedFile */
    _priv->b6t_mapped = g_mapped_file_new(filenames[0], FALSE, &local_error);
    if (!_priv->b6t_mapped) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to map file '%s': %s!\n", __func__, filenames[0], local_error->message);
        g_error_free(local_error);
        mirage_error(MIRAGE_E_IMAGEFILE, error);
        succeeded = FALSE;
        goto end;
    }
    
    _priv->b6t_data = (guint8 *)g_mapped_file_get_contents(_priv->b6t_mapped);
    
    /* Load parser */
    succeeded = __mirage_parser_b6t_load_parser(self, error);
    if (!succeeded) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to load parser!\n", __func__);
    }
    
    _priv->b6t_data = NULL;
    g_mapped_file_free(_priv->b6t_mapped);

end:    
    /* Return disc */
    mirage_object_detach_child(MIRAGE_OBJECT(self), _priv->disc, NULL);
    if (succeeded) {
        *disc = _priv->disc;
    } else {
        g_object_unref(_priv->disc);
        *disc = NULL;
    }
        
    return succeeded;
}


/******************************************************************************\
 *                                Object init                                 *
\******************************************************************************/
/* Our parent class */
static MIRAGE_ParserClass *parent_class = NULL;

static void __mirage_parser_b6t_instance_init (GTypeInstance *instance, gpointer g_class) {
    mirage_parser_generate_parser_info(MIRAGE_PARSER(instance),
        "PARSER-B6T",
        "B6T Image Parser",
        PACKAGE_VERSION,
        "Rok Mandeljc",
        FALSE,
        "BlindWrite 5/6 images",
        3, ".b5t", ".b6t", NULL
    );
    
    return;
}

static void __mirage_parser_b6t_finalize (GObject *obj) {
    MIRAGE_Parser_B6T *self = MIRAGE_PARSER_B6T(obj);
    MIRAGE_Parser_B6TPrivate *_priv = MIRAGE_PARSER_B6T_GET_PRIVATE(self);
    GList *entry;
    
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_GOBJECT, "%s:\n", __func__);
    
    /* Free list of data blocks */
    G_LIST_FOR_EACH(entry, _priv->data_blocks_list) {        
        if (entry->data) {
            B6T_DataBlock *data_block = entry->data;
            /* Free filename */
            g_free(data_block->filename);
            /* Free the data block */
            g_free(data_block);
        }
    }
    g_list_free(_priv->data_blocks_list);
    
    g_free(_priv->b6t_filename);
    g_free(_priv->cdtext_data);
    
    /* Chain up to the parent class */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_GOBJECT, "%s: chaining up to parent\n", __func__);
    return G_OBJECT_CLASS(parent_class)->finalize(obj);
}


static void __mirage_parser_b6t_class_init (gpointer g_class, gpointer g_class_data) {
    GObjectClass *class_gobject = G_OBJECT_CLASS(g_class);
    MIRAGE_ParserClass *class_parser = MIRAGE_PARSER_CLASS(g_class);
    MIRAGE_Parser_B6TClass *klass = MIRAGE_PARSER_B6T_CLASS(g_class);
    
    /* Set parent class */
    parent_class = g_type_class_peek_parent(klass);
    
    /* Register private structure */
    g_type_class_add_private(klass, sizeof(MIRAGE_Parser_B6TPrivate));
    
    /* Initialize GObject members */
    class_gobject->finalize = __mirage_parser_b6t_finalize;
    
    /* Initialize MIRAGE_Parser members */
    class_parser->load_image = __mirage_parser_b6t_load_image;
        
    return;
}

GType mirage_parser_b6t_get_type (GTypeModule *module) {
    static GType type = 0;
    if (type == 0) {
        static const GTypeInfo info = {
            sizeof(MIRAGE_Parser_B6TClass),
            NULL,   /* base_init */
            NULL,   /* base_finalize */
            __mirage_parser_b6t_class_init,   /* class_init */
            NULL,   /* class_finalize */
            NULL,   /* class_data */
            sizeof(MIRAGE_Parser_B6T),
            0,      /* n_preallocs */
            __mirage_parser_b6t_instance_init    /* instance_init */
        };
        
        type = g_type_module_register_type(module, MIRAGE_TYPE_PARSER, "MIRAGE_Parser_B6T", &info, 0);
    }
    
    return type;
}
