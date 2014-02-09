/*
 *  libMirage: NRG image parser: Parser object
 *  Copyright (C) 2006-2012 Rok Mandeljc
 *
 *  Reverse-engineering work in March, 2005 by Henrik Stokseth.
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

#include "image-nrg.h"

#define __debug__ "NRG-Parser"


static const guint8 nero_signature[4] = { 'N', 'E', 'R', 'O' };
static const guint8 ner5_signature[4] = { 'N', 'E', 'R', '5' };


/**********************************************************************\
 *                          Private structure                         *
\**********************************************************************/
#define MIRAGE_PARSER_NRG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), MIRAGE_TYPE_PARSER_NRG, MirageParserNrgPrivate))

struct _MirageParserNrgPrivate
{
    MirageDisc *disc;

    MirageStream *nrg_stream;

    GList *block_index;
    gint  block_index_entries;

    gboolean old_format;

    guint8 *nrg_data;
    guint  nrg_data_length;

    gint32 prev_session_end;

    NRG_ETN_Block *etn_blocks;
    gint num_etn_blocks;

    NRG_CUE_Block *cue_blocks;
    gint num_cue_blocks;

    NRG_DAO_Header *dao_header;
    NRG_DAO_Block *dao_blocks;
    gint num_dao_blocks;
};


typedef struct
{
    gchar *block_id;
    gint subblock_offset;
    gint subblock_length;
} NRG_BlockIDs;

/* NULL terminated list of valid block IDs and subblock offset + length */
static NRG_BlockIDs NRGBlockID[] = {
    { "CUEX", 0,  8  },
    { "CUES", 0,  8  },
    { "ETN2", 0,  32 },
    { "ETNF", 0,  20 },
    { "DAOX", 22, 42 },
    { "DAOI", 22, 30 },
    { "CDTX", 0,  0  },
    { "SINF", 0,  0  },
    { "MTYP", 0,  0  },
    { "END!", 0,  0  },
    { NULL,   0,  0  }
};

static gboolean mirage_parser_nrg_build_block_index (MirageParserNrg *self, GError **error)
{
    NRGBlockIndexEntry *blockentry;
    GList *blockindex = NULL;
    gint num_blocks;
    gint index;
    guint8 *cur_ptr;

    /* Populate block index */
    cur_ptr = self->priv->nrg_data;
    index = 0;
    do {
        blockentry = g_new0(NRGBlockIndexEntry, 1);
        if (!blockentry) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to allocate memory for block index!\n", __debug__);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Failed to allocate memory for block index!");
            return FALSE;
        }
        blockentry->offset = (guint64) (cur_ptr - self->priv->nrg_data);
        memcpy(blockentry->block_id, cur_ptr, 4);
        cur_ptr += 4;
        blockentry->length = GINT32_FROM_BE(MIRAGE_CAST_DATA(cur_ptr, 0, guint32));
        cur_ptr += sizeof(guint32);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: block %2i, ID: %.4s, offset: %Li (0x%LX), length: %i (0x%X).\n", \
        __debug__, index, blockentry->block_id, blockentry->offset, blockentry->offset, blockentry->length, blockentry->length);

        /* Got sub-blocks? */
        for (gint id_index = 0; NRGBlockID[id_index].block_id; id_index++) {
            if (!memcmp(blockentry->block_id, NRGBlockID[id_index].block_id, 4)) {
                if (NRGBlockID[id_index].subblock_length) {
                    blockentry->subblocks_offset = blockentry->offset + 8 + NRGBlockID[id_index].subblock_offset;
                    blockentry->subblocks_length = NRGBlockID[id_index].subblock_length;
                    blockentry->num_subblocks = (blockentry->length - NRGBlockID[id_index].subblock_offset) / blockentry->subblocks_length;
                    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:   Sub-blocks: %i, blocksize: %i.\n", __debug__, blockentry->num_subblocks, blockentry->subblocks_length);
                } else {
                    /* This block has no sub-blocks */
                    blockentry->subblocks_offset = 0;
                    blockentry->subblocks_length = 0;
                    blockentry->num_subblocks = 0;
                }
                break;
            }
        }

        /* Add entry to list */
        blockindex = g_list_prepend(blockindex, blockentry);

        /* Get ready for next block */
        index++;
        cur_ptr += blockentry->length;
    } while (cur_ptr < self->priv->nrg_data + self->priv->nrg_data_length);

    blockindex = g_list_reverse(blockindex);
    num_blocks = g_list_length(blockindex);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: counted %i blocks.\n", __debug__, num_blocks);

    /* Link to block index */
    self->priv->block_index = blockindex;
    self->priv->block_index_entries = num_blocks;

    return TRUE;
}

static gboolean mirage_parser_nrg_destroy_block_index (MirageParserNrg *self)
{
    if (self->priv->block_index) {
        for (GList *entry = self->priv->block_index; entry; entry = entry->next) {
            g_free(entry->data);
        }
        g_list_free(self->priv->block_index);
        self->priv->block_index = NULL;
        self->priv->block_index_entries = 0;
    }

    return TRUE;
}

static gint find_by_block_id (const NRGBlockIndexEntry *blockentry, const gchar *block_id)
{
    return memcmp(blockentry->block_id, block_id, 4);
}

static NRGBlockIndexEntry *mirage_parser_nrg_find_block_entry (MirageParserNrg *self, gchar *block_id, gint index)
{
    GList *list_start = self->priv->block_index;
    gint cur_index = 0;

    do {
        GList *list_entry = g_list_find_custom(list_start, block_id, (GCompareFunc)find_by_block_id);

        if (list_entry) {
            if (cur_index == index) {
                return list_entry->data;
            } else {
                if (list_entry->next) {
                    list_start = list_entry->next;
                } else {
                    break;
                }
            }
        } else {
            break;
        }
    } while (cur_index++ < index);

    return NULL;
}

static gboolean mirage_parser_nrg_load_medium_type (MirageParserNrg *self, GError **error)
{
    NRGBlockIndexEntry *blockentry;
    guint8 *cur_ptr;
    guint32 mtyp_data;

    /* Look up MTYP block */
    blockentry = mirage_parser_nrg_find_block_entry(self, "MTYP", 0);
    if (!blockentry) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to look up 'MTYP' block!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Failed to look up 'MTYP' block!");
        return FALSE;
    }
    cur_ptr = self->priv->nrg_data + blockentry->offset + 8;

    mtyp_data = GINT32_FROM_BE(MIRAGE_CAST_DATA(cur_ptr, 0, guint32));
    cur_ptr += sizeof(guint32);

    /* Decode medium type */
    NERO_MEDIA_TYPE CD_EQUIV = MEDIA_CD | MEDIA_CDROM;
    NERO_MEDIA_TYPE DVD_EQUIV = MEDIA_DVD_ANY | MEDIA_DVD_ROM;
    NERO_MEDIA_TYPE BD_EQUIV = MEDIA_BD_ANY;
    NERO_MEDIA_TYPE HD_EQUIV = MEDIA_HD_DVD_ANY;

    if (mtyp_data & CD_EQUIV) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: medium type: CD-ROM\n", __debug__);
        mirage_disc_set_medium_type(self->priv->disc, MIRAGE_MEDIUM_CD);
    } else if (mtyp_data & DVD_EQUIV) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: medium type: DVD-ROM\n", __debug__);
        mirage_disc_set_medium_type(self->priv->disc, MIRAGE_MEDIUM_DVD);
    } else if (mtyp_data & BD_EQUIV) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: medium type: Blue-ray\n", __debug__);
        mirage_disc_set_medium_type(self->priv->disc, MIRAGE_MEDIUM_BD);
    } else if (mtyp_data & HD_EQUIV) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: medium type: HD-DVD\n", __debug__);
        mirage_disc_set_medium_type(self->priv->disc, MIRAGE_MEDIUM_HD);
    } else {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: unhandled medium type: %d!\n", __debug__, mtyp_data);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Unhandled medium type: %d!", mtyp_data);
        return FALSE;
    }

    return TRUE;
}

static void mirage_parser_nrg_decode_mode (MirageParserNrg *self, gint code, gint *mode, gint *main_sectsize, gint *sub_sectsize)
{
    /* The meaning of the following codes was determined experimentally; we're
       missing mappings for Mode 2 Formless, but that doesn't seem a very common
       format anyway */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: mode code: %d\n", __debug__, code);
    switch (code) {
        case 0x00: {
            /* Mode 1, user data only */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 1, user data only\n", __debug__, code);
            *mode = MIRAGE_SECTOR_MODE1;
            *main_sectsize = 2048;
            *sub_sectsize = 0;
            break;
        }
        case 0x02: {
            /* Mode 2 Form 1, user data only */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 2 Form 1, user data only\n", __debug__, code);
            *mode = MIRAGE_SECTOR_MODE2_FORM1;
            *main_sectsize = 2048;
            *sub_sectsize = 0;
            break;
        }
        case 0x03: {
            /* Mode 2 Form 2, user data only */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 2 Form 2, user data only\n", __debug__, code);
            *mode = MIRAGE_SECTOR_MODE2_FORM2;
            *main_sectsize = 2336;
            *sub_sectsize = 0;
            break;
        }
        case 0x05: {
            /* Mode 1, full sector */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 1, full sector\n", __debug__, code);
            *mode = MIRAGE_SECTOR_MODE1;
            *main_sectsize = 2352;
            *sub_sectsize = 0;
            break;
        }
        case 0x06: {
            /* Mode 2 Form 1 or Mode 2 Form 2, full sector */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 2 Form 1/2, full sector\n", __debug__, code);
            *mode = MIRAGE_SECTOR_MODE2_MIXED;
            *main_sectsize = 2352;
            *sub_sectsize = 0;
            break;
        }
        case 0x07: {
            /* Audio, full sector */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Audio, full sector\n", __debug__, code);
            *mode = MIRAGE_SECTOR_AUDIO;
            *main_sectsize = 2352;
            *sub_sectsize = 0;
            break;
        }
        case 0x0F: {
             /* Mode 1, full sector with subchannel */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 1, full sector with subchannel\n", __debug__, code);
            *mode = MIRAGE_SECTOR_MODE1;
            *main_sectsize = 2352;
            *sub_sectsize = 96;
            break;
        }
        case 0x10: {
            /* Audio, full sector with subchannel */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Audio, full sector with subchannel\n", __debug__, code);
            *mode = MIRAGE_SECTOR_AUDIO;
            *main_sectsize = 2352;
            *sub_sectsize = 96;
            break;
        }
        case 0x11: {
            /* Mode 2 Form 1 or Mode 2 Form 2, full sector with subchannel */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: Mode 2 Form 1/2, full sector with subchannel\n", __debug__, code);
            *mode = MIRAGE_SECTOR_MODE2_MIXED;
            *main_sectsize = 2352;
            *sub_sectsize = 96;
            break;
        }
        default: {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: unknown mode code: %d! Assuming sector size 2352 and no subchannel!\n", __debug__, code);
            *mode = MIRAGE_SECTOR_AUDIO;
            *main_sectsize = 2352;
            *sub_sectsize = 0;
            break;
        }
    }
}

static gboolean mirage_parser_nrg_load_etn_data (MirageParserNrg *self, gint session_num, GError **error)
{
    NRGBlockIndexEntry *blockentry;
    guint8 *cur_ptr;

    /* Look up ETN2 / ETNF block */
    if (!self->priv->old_format) {
        blockentry = mirage_parser_nrg_find_block_entry(self, "ETN2", session_num);
    } else {
        blockentry = mirage_parser_nrg_find_block_entry(self, "ETNF", session_num);
    }
    if (!blockentry) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to look up 'ETN2' or 'ETNF' block!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Failed to look up 'ETN2' or 'ETNF' block!");
        return FALSE;
    }
    cur_ptr = self->priv->nrg_data + blockentry->offset + 8;

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: %d ETN blocks\n", __debug__, blockentry->num_subblocks);
    self->priv->num_etn_blocks = blockentry->num_subblocks;

    /* Allocate space and read ETN data (we need to copy data because we'll have
       to modify it) */
    self->priv->etn_blocks = g_try_new0(NRG_ETN_Block, blockentry->num_subblocks);
    if (!self->priv->etn_blocks) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to allocate space for ETN blocks!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Failed to allocate space for ETN blocks!");
        return FALSE;
    }

    /* Read ETN blocks */
    for (gint i = 0; i < blockentry->num_subblocks; i++) {
        NRG_ETN_Block *block = &self->priv->etn_blocks[i];

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: ETN block #%i\n", __debug__, i);

        if (self->priv->old_format) {
            block->offset = GINT32_FROM_BE(MIRAGE_CAST_DATA(cur_ptr, 0, guint32));
            block->size = GINT32_FROM_BE(MIRAGE_CAST_DATA(cur_ptr, 4, guint32));
            block->mode = MIRAGE_CAST_DATA(cur_ptr, 11, guint8);
            block->sector = GINT32_FROM_BE(MIRAGE_CAST_DATA(cur_ptr, 12, guint32));
        }
        else {
            block->offset = GINT64_FROM_BE(MIRAGE_CAST_DATA(cur_ptr, 0, guint64));
            block->size = GINT64_FROM_BE(MIRAGE_CAST_DATA(cur_ptr, 8, guint64));
            block->mode = MIRAGE_CAST_DATA(cur_ptr, 19, guint8);
            block->sector = GINT32_FROM_BE(MIRAGE_CAST_DATA(cur_ptr, 20, guint32));
        }
        cur_ptr += blockentry->subblocks_length;

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  offset: %u\n", __debug__, block->offset);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  size: %u\n", __debug__, block->size);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  mode: 0x%X\n", __debug__, block->mode);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  sector: %u\n\n", __debug__, block->sector);
    }

    return TRUE;
}

static gboolean mirage_parser_nrg_load_cue_data (MirageParserNrg *self, gint session_num, GError **error)
{
    NRGBlockIndexEntry *blockentry;
    guint8 *cur_ptr;

    /* Look up CUEX / CUES block */
    if (!self->priv->old_format) {
        blockentry = mirage_parser_nrg_find_block_entry(self, "CUEX", session_num);
    } else {
        blockentry = mirage_parser_nrg_find_block_entry(self, "CUES", session_num);
    }
    if (!blockentry) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to look up 'CUEX' or 'CUES' block!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Failed to look up 'CUEX' or 'CUES' block!");
        return FALSE;
    }
    cur_ptr = self->priv->nrg_data + blockentry->offset + 8;

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: %d CUE blocks\n", __debug__, blockentry->num_subblocks);
    self->priv->num_cue_blocks = blockentry->num_subblocks;

    /* Allocate space and read CUE data (we need to copy data because we'll have
       to modify it) */
    self->priv->cue_blocks = g_try_new0(NRG_CUE_Block, blockentry->num_subblocks);
    if (!self->priv->cue_blocks) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to allocate space for CUE blocks!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Failed to allocate memory for CUE blocks!");
        return FALSE;
    }

    /* Read CUE blocks */
    memcpy(self->priv->cue_blocks, MIRAGE_CAST_PTR(cur_ptr, 0, guint8 *), blockentry->length);
    cur_ptr += blockentry->length;

    /* Conversion */
    for (gint i = 0; i < blockentry->num_subblocks; i++) {
        NRG_CUE_Block *block = &self->priv->cue_blocks[i];

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: CUE block #%i\n", __debug__, i);

        /* BCD -> Hex */
        block->track = mirage_helper_bcd2hex(block->track);
        block->index = mirage_helper_bcd2hex(block->index);

        /* Old format has MSF format, new one has Big-endian LBA */
        if (self->priv->old_format) {
            guint8 *hmsf = (guint8 *)&block->start_sector;
            block->start_sector = mirage_helper_msf2lba(hmsf[1], hmsf[2], hmsf[3], TRUE);
        } else {
            block->start_sector = GUINT32_FROM_BE(block->start_sector);
        }

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  adr/ctl: 0x%X\n", __debug__, block->adr_ctl);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  track: 0x%X\n", __debug__, block->track);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  index: 0x%X\n", __debug__, block->index);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  start_sector: 0x%X\n\n", __debug__, block->start_sector);
    }

    return TRUE;
}

static gboolean mirage_parser_nrg_load_dao_data (MirageParserNrg *self, gint session_num, GError **error)
{
    NRGBlockIndexEntry *blockentry;
    guint8 *cur_ptr;

    /* Look up DAOX / DAOI block */
    if (!self->priv->old_format) {
        blockentry = mirage_parser_nrg_find_block_entry(self, "DAOX", session_num);
    } else {
        blockentry = mirage_parser_nrg_find_block_entry(self, "DAOI", session_num);
    }
    if (!blockentry) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to look up 'DAOX' or 'DAOI' block!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Failed to look up 'DAOX' or 'DAOI' block!");
        return FALSE;
    }
    cur_ptr = self->priv->nrg_data + blockentry->offset + 8;

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: %d DAO blocks\n", __debug__, blockentry->num_subblocks);
    self->priv->num_dao_blocks = blockentry->num_subblocks;

    /* Allocate space and read DAO header */
    self->priv->dao_header = g_try_new0(NRG_DAO_Header, 1);
    if (!self->priv->dao_header) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to allocate space for DAO header!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Failed to allocate space for DAO header!");
        return FALSE;
    }
    memcpy(self->priv->dao_header, MIRAGE_CAST_PTR(cur_ptr, 0, NRG_DAO_Header *), sizeof(NRG_DAO_Header));
    cur_ptr += sizeof(NRG_DAO_Header);

    /* Allocate space and read DAO blocks */
    self->priv->dao_blocks = g_new0(NRG_DAO_Block, blockentry->num_subblocks);
    for (gint i = 0; i < blockentry->num_subblocks; i++) {
        NRG_DAO_Block *block = &self->priv->dao_blocks[i];

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: DAO block #%i\n", __debug__, i);

        /* We read each block separately because the last fields are different
           between formats */
        memcpy(block, MIRAGE_CAST_PTR(cur_ptr, 0, guint8 *), 18); /* First 18 bytes are common */
        cur_ptr += 18;

        /* Handle big-endianess */
        block->sector_size = GUINT16_FROM_BE(block->sector_size);

        if (self->priv->old_format) {
            gint32 *tmp_int = MIRAGE_CAST_PTR(cur_ptr, 0, gint32 *);
            cur_ptr += 3 * sizeof(gint32);
            /* Conversion */
            block->pregap_offset = GINT32_FROM_BE(tmp_int[0]);
            block->start_offset = GINT32_FROM_BE(tmp_int[1]);
            block->end_offset = GINT32_FROM_BE(tmp_int[2]);
        } else {
            gint64 *tmp_int = MIRAGE_CAST_PTR(cur_ptr, 0, gint64 *);
            cur_ptr += 3 * sizeof(gint64);
            /* Conversion */
            block->pregap_offset = GINT64_FROM_BE(tmp_int[0]);
            block->start_offset = GINT64_FROM_BE(tmp_int[1]);
            block->end_offset = GINT64_FROM_BE(tmp_int[2]);
        }

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  ISRC: %.12s\n", __debug__, block->isrc);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  mode code: 0x%X\n", __debug__, block->mode_code);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  sector size: 0x%X\n", __debug__, block->sector_size);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  pregap offset: 0x%llX\n", __debug__, block->pregap_offset);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  start offset: 0x%llX\n", __debug__, block->start_offset);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s:  end offset: 0x%llX\n\n", __debug__, block->end_offset);
    }

    return TRUE;
}

static gboolean mirage_parser_nrg_load_session (MirageParserNrg *self, gint session_num, GError **error)
{
    gboolean succeeded = TRUE;

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: loading session\n", __debug__);

    /* Read CUEX/CUES blocks */
    if (!mirage_parser_nrg_load_cue_data(self, session_num, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to load CUE blocks!\n", __debug__);
        succeeded = FALSE;
        goto end;
    }

    /* Read DAOX/DAOI blocks */
    if (!mirage_parser_nrg_load_dao_data(self, session_num, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to load DAO blocks!\n", __debug__);
        succeeded = FALSE;
        goto end;
    }

    /* Build session */
    MirageSession *session;

    session = g_object_new(MIRAGE_TYPE_SESSION, NULL);
    mirage_disc_add_session_by_index(self->priv->disc, -1, session);

    /* Set MCN */
    if (strlen(self->priv->dao_header->mcn)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: MCN: %.13s\n", __debug__, self->priv->dao_header->mcn);
        mirage_session_set_mcn(session, self->priv->dao_header->mcn);
    }

    /* Use DAO blocks to build tracks */
    for (gint i = 0; i < self->priv->num_dao_blocks; i++) {
        NRG_DAO_Block *dao_block = self->priv->dao_blocks + i;

        MirageTrack *track;
        gint mode = 0;
        gint main_sectsize = 0;
        gint sub_sectsize = 0;

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: creating track for DAO block #%i\n", __debug__, i);

        /* Add track */
        track = g_object_new(MIRAGE_TYPE_TRACK, NULL);
        mirage_session_add_track_by_index(session, i, track);

        /* Decode mode */
        mirage_parser_nrg_decode_mode(self, dao_block->mode_code, &mode, &main_sectsize, &sub_sectsize);
        mirage_track_set_sector_type(track, mode);

        /* Shouldn't happen, but just in case I misinterpreted something */
        if (main_sectsize + sub_sectsize != dao_block->sector_size) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: sector size mismatch (%i vs %i)!\n", __debug__, main_sectsize + sub_sectsize, dao_block->sector_size);
        }

        /* Prepare data fragment: we use two fragments, one for pregap and one
           for track itself; we could use only one that spans across both, but
           I'm not sure why image file has the offsets separated - maybe they
           don't have to be adjacent?
        */
        gint main_size;
        guint64 main_offset;
        guint64 main_format;

        gint subchannel_size;
        guint64 subchannel_format;

        gint fragment_len;

        MirageFragment *fragment;

        /* Pregap fragment */
        fragment_len = (dao_block->start_offset - dao_block->pregap_offset) / dao_block->sector_size;
        if (fragment_len) {
            /* Create a binary fragment */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: creating pregap fragment\n", __debug__);

            fragment = g_object_new(MIRAGE_TYPE_FRAGMENT, NULL);

            /* Main channel data */
            main_size = main_sectsize; /* We use the one from decoded mode code */
            main_offset = dao_block->pregap_offset;
            if (mode == MIRAGE_SECTOR_AUDIO) {
                main_format = MIRAGE_MAIN_DATA_FORMAT_AUDIO;
            } else {
                main_format = MIRAGE_MAIN_DATA_FORMAT_DATA;
            }

            mirage_fragment_set_length(fragment, fragment_len);

            mirage_fragment_main_data_set_stream(fragment, self->priv->nrg_stream);
            mirage_fragment_main_data_set_offset(fragment, main_offset);
            mirage_fragment_main_data_set_size(fragment, main_size);
            mirage_fragment_main_data_set_format(fragment, main_format);

            /* Subchannel */
            if (sub_sectsize) {
                subchannel_size = sub_sectsize; /* We use the one from decoded mode code */
                subchannel_format = MIRAGE_SUBCHANNEL_DATA_FORMAT_PW96_INTERLEAVED | MIRAGE_SUBCHANNEL_DATA_FORMAT_INTERNAL; /* PW96 interleaved, internal */

                mirage_fragment_subchannel_data_set_size(fragment, subchannel_size);
                mirage_fragment_subchannel_data_set_format(fragment, subchannel_format);
            }

            mirage_track_add_fragment(track, -1, fragment);

            g_object_unref(fragment);
        }

        /* Data fragment */
        fragment_len = (dao_block->end_offset - dao_block->start_offset) / dao_block->sector_size;
        if (fragment_len) {
            /* Create a binary fragment */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: creating data fragment\n", __debug__);

            fragment = g_object_new(MIRAGE_TYPE_FRAGMENT, NULL);

            /* Main channel data */
            main_size = main_sectsize; /* We use the one from decoded mode code */
            main_offset = dao_block->start_offset;
            if (mode == MIRAGE_SECTOR_AUDIO) {
                main_format = MIRAGE_MAIN_DATA_FORMAT_AUDIO;
            } else {
                main_format = MIRAGE_MAIN_DATA_FORMAT_DATA;
            }

            mirage_fragment_set_length(fragment, fragment_len);

            mirage_fragment_main_data_set_stream(fragment, self->priv->nrg_stream);
            mirage_fragment_main_data_set_offset(fragment, main_offset);
            mirage_fragment_main_data_set_size(fragment, main_size);
            mirage_fragment_main_data_set_format(fragment, main_format);

            /* Subchannel */
            if (sub_sectsize) {
                subchannel_size = sub_sectsize; /* We use the one from decoded mode code */
                subchannel_format = MIRAGE_SUBCHANNEL_DATA_FORMAT_PW96_INTERLEAVED | MIRAGE_SUBCHANNEL_DATA_FORMAT_INTERNAL; /* PW96 interleaved, internal */

                mirage_fragment_subchannel_data_set_size(fragment, subchannel_size);
                mirage_fragment_subchannel_data_set_format(fragment, subchannel_format);
            }

            mirage_track_add_fragment(track, -1, fragment);

            g_object_unref(fragment);
        }

        /* Validate and set ISRC */
        if (mirage_helper_validate_isrc(dao_block->isrc)){
            mirage_track_set_isrc(track, dao_block->isrc);
        }

        g_object_unref(track);
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");
    }

    /* Use CUE blocks to set pregaps and indices */
    gint track_start = 0;
    for (gint i = 0; i < self->priv->num_cue_blocks; i++) {
        NRG_CUE_Block *cue_block = self->priv->cue_blocks + i;

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: track %i, index %i, sector 0x%X\n", __debug__, cue_block->track, cue_block->index, cue_block->start_sector);

        /* Track 0, index 0... lead-in */
        if (cue_block->track == 0 && cue_block->index == 0) {
            /* Try to get previous session; if we fail, it means this if first
               session, so we use its lead-in block to set parser start; otherwise,
               we calculate length of previous session's leadout based on this
               session's start sector and length of parser so far */
            MirageSession *prev_session;

            /* Index is -2, because current session has already been added */
            prev_session = mirage_disc_get_session_by_index(self->priv->disc, -2, NULL);
            if (!prev_session) {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: lead-in block of first session; setting parser start to 0x%X\n", __debug__, cue_block->start_sector);
                mirage_disc_layout_set_start_sector(self->priv->disc, cue_block->start_sector);
            } else {
                gint leadout_length = 0;

                leadout_length = cue_block->start_sector - self->priv->prev_session_end;

                MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: lead-in block; setting previous session's leadout length to 0x%X\n", __debug__, leadout_length);

                mirage_session_set_leadout_length(prev_session, leadout_length);

                g_object_unref(prev_session);
            }
        } else if (cue_block->track == 170) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: lead-out block\n", __debug__);
            self->priv->prev_session_end = cue_block->start_sector;
        } else {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: track entry\n", __debug__);
            if (cue_block->index == 0) {
                track_start = cue_block->start_sector;
            } else {
                MirageTrack *track;
                gint cur_address = cue_block->start_sector - track_start;

                track = mirage_session_get_track_by_number(session, cue_block->track, NULL);

                if (cue_block->index == 1) {
                    /* Track start */
                    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: setting index 1 to address 0x%X\n", __debug__, cur_address);
                    mirage_track_set_track_start(track, cur_address);
                } else {
                    /* Additional index */
                    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: adding index at address 0x%X\n", __debug__, cur_address);
                    mirage_track_add_index(track, cur_address, NULL);
                }

                g_object_unref(track);
            }
        }
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "\n");
    }

    g_object_unref(session);

end:
    /* Free data */
    g_free(self->priv->cue_blocks);
    self->priv->num_cue_blocks = 0;
    g_free(self->priv->dao_header);
    g_free(self->priv->dao_blocks);
    self->priv->num_dao_blocks = 0;

    return succeeded;
}

static gboolean mirage_parser_nrg_load_session_tao (MirageParserNrg *self, gint session_num, GError **error)
{
    gboolean succeeded = TRUE;

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: loading session (TAO)\n", __debug__);

    /* Read ETNF/ETN2 blocks */
    if (!mirage_parser_nrg_load_etn_data(self, session_num, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to load ETNF/ETN2 blocks!\n", __debug__);
        succeeded = FALSE;
        goto end;
    }

    /* Build session */
    MirageSession *session;

    session = g_object_new(MIRAGE_TYPE_SESSION, NULL);
    mirage_disc_add_session_by_index(self->priv->disc, -1, session);

    /* Use ETN blocks to build tracks */
    for (gint i = 0; i < self->priv->num_etn_blocks; i++) {
        NRG_ETN_Block *etn_block = self->priv->etn_blocks + i;

        MirageTrack *track;
        gint mode = 0;
        gint main_sectsize = 0;
        gint sub_sectsize = 0;

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: creating track for ETN block #%i\n", __debug__, i);

        /* Add track */
        track = g_object_new(MIRAGE_TYPE_TRACK, NULL);
        mirage_session_add_track_by_index(session, i, track);

        /* Decode mode */
        mirage_parser_nrg_decode_mode(self, etn_block->mode, &mode, &main_sectsize, &sub_sectsize);
        mirage_track_set_sector_type(track, mode);

        /* Prepare data fragment: we use two fragments, one for pregap and one
           for track itself; we could use only one that spans across both, but
           I'm not sure why image file has the offsets separated - maybe they
           don't have to be adjacent?
        */
        gint main_size;
        guint64 main_offset;
        guint64 main_format;

        gint subchannel_size;
        guint64 subchannel_format;

        gint fragment_len;

        MirageFragment *fragment;

        /* Pregap fragment - NULL fragment */
        fragment = g_object_new(MIRAGE_TYPE_FRAGMENT, NULL);

        mirage_track_add_fragment(track, 0, fragment);
        mirage_fragment_set_length(fragment, 150);

        g_object_unref(fragment);

        mirage_track_set_track_start(track, 150);

        /* Data fragment */
        fragment_len = etn_block->size / main_sectsize;
        if (fragment_len) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: creating data fragment\n", __debug__);

            fragment = g_object_new(MIRAGE_TYPE_FRAGMENT, NULL);

            /* Main channel data */
            main_size = main_sectsize; /* We use the one from decoded mode code */
            main_offset = etn_block->offset;
            if (mode == MIRAGE_SECTOR_AUDIO) {
                main_format = MIRAGE_MAIN_DATA_FORMAT_AUDIO;
            } else {
                main_format = MIRAGE_MAIN_DATA_FORMAT_DATA;
            }

            mirage_fragment_set_length(fragment, fragment_len);

            mirage_fragment_main_data_set_stream(fragment, self->priv->nrg_stream);
            mirage_fragment_main_data_set_offset(fragment, main_offset);
            mirage_fragment_main_data_set_size(fragment, main_size);
            mirage_fragment_main_data_set_format(fragment, main_format);

            /* Subchannel */
            if (sub_sectsize) {
                subchannel_size = sub_sectsize; /* We use the one from decoded mode code */
                subchannel_format = MIRAGE_SUBCHANNEL_DATA_FORMAT_PW96_INTERLEAVED | MIRAGE_SUBCHANNEL_DATA_FORMAT_INTERNAL; /* PW96 interleaved, internal */

                mirage_fragment_subchannel_data_set_size(fragment, subchannel_size);
                mirage_fragment_subchannel_data_set_format(fragment, subchannel_format);
            }

            mirage_track_add_fragment(track, -1, fragment);

            g_object_unref(fragment);
        }

        g_object_unref(track);
    }

    g_object_unref(session);

end:
    /* Free data */
    g_free(self->priv->etn_blocks);
    self->priv->num_etn_blocks = 0;

    return succeeded;
}

static gboolean mirage_parser_nrg_load_cdtext (MirageParserNrg *self, GError **error)
{
    NRGBlockIndexEntry *blockentry;
    guint8 *cur_ptr;
    guint8 *cdtx_data;
    MirageSession *session;
    gboolean succeeded = TRUE;

    /* Look up CDTX block */
    blockentry = mirage_parser_nrg_find_block_entry(self, "CDTX", 0);
    if (!blockentry) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to look up 'CDTX' block!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_PARSER_ERROR, "Failed to look up 'CDTX' block!");
        return FALSE;
    }
    cur_ptr = self->priv->nrg_data + blockentry->offset + 8;

    /* Read CDTX data */
    cdtx_data = cur_ptr;

    session = mirage_disc_get_session_by_index(self->priv->disc, 0, error);
    if (session) {
        if (!mirage_session_set_cdtext_data(session, cdtx_data, blockentry->length, error)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to set CD-TEXT data!\n", __debug__);
            succeeded = FALSE;
        }
        g_object_unref(session);
    } else {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to get session!\n", __debug__);
        succeeded = FALSE;
    }

    return succeeded;
}


/**********************************************************************\
 *                MirageParser methods implementation                *
\**********************************************************************/
static MirageDisc *mirage_parser_nrg_load_image (MirageParser *_self, MirageStream **streams, GError **error)
{
    MirageParserNrg *self = MIRAGE_PARSER_NRG(_self);
    const gchar *nrg_filename;
    gboolean succeeded = TRUE;
    guint64 file_size;
    guint64 trailer_offset;
    gchar sig[4];

    /* Open file */
    self->priv->nrg_stream = g_object_ref(streams[0]);

    /* Get file size */
    mirage_stream_seek(self->priv->nrg_stream, 0, G_SEEK_END, NULL);
    file_size = mirage_stream_tell(self->priv->nrg_stream);

    /* Read signature */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: checking if parser can handle given image...\n", __debug__);
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: checking for new Nero image signature (NER5) at 64-bit offset from the end of file!\n", __debug__);

    mirage_stream_seek(self->priv->nrg_stream, -12, G_SEEK_END, NULL);

    if (mirage_stream_read(self->priv->nrg_stream, sig, sizeof(sig), NULL) != sizeof(sig)) {
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_CANNOT_HANDLE, "Parser cannot handle given image: failed to read signature!");
        return FALSE;
    }

    if (!memcmp(sig, ner5_signature, sizeof(ner5_signature))) {
        /* New format, 64-bit offset */
        guint64 tmp_offset = 0;
        self->priv->old_format = FALSE;

        MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: new Nero image signature (NER5) found!\n", __debug__);

        if (mirage_stream_read(self->priv->nrg_stream, &tmp_offset, sizeof(tmp_offset), NULL) != sizeof(tmp_offset)) {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to read trailer offset (64-bit)!\n", __debug__);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to read trailer offset!");
            return FALSE;
        }
        trailer_offset = GUINT64_FROM_BE(tmp_offset);
        self->priv->nrg_data_length = (file_size - 12) - trailer_offset;
    } else {
        /* Try old format, with 32-bit offset */
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: checking for old Nero image signature (NERO) at 32-bit offset from the end of file!\n", __debug__);

        mirage_stream_seek(self->priv->nrg_stream, -8, G_SEEK_END, NULL);
        if (mirage_stream_read(self->priv->nrg_stream, sig, sizeof(sig), NULL) != sizeof(sig)) {
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_CANNOT_HANDLE, "Parser cannot handle given image: failed to read signature!");
            return FALSE;
        }

        if (!memcmp(sig, nero_signature, sizeof(nero_signature))) {
            guint32 tmp_offset = 0;
            self->priv->old_format = TRUE;

            MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: old Nero image signature (NERO) found!\n", __debug__);

            if (mirage_stream_read(self->priv->nrg_stream, &tmp_offset, sizeof(tmp_offset), NULL) != sizeof(tmp_offset)) {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to read trailer offset!\n", __debug__);
                g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to seek to trailer offset (32-bit)!");
                return FALSE;
            }

            trailer_offset = GUINT32_FROM_BE(tmp_offset);
            self->priv->nrg_data_length = (file_size - 8) - trailer_offset;
        } else {
            /* Unknown signature, can't handle the file */
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: parser cannot handle given image: invalid signature!\n", __debug__);
            g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_CANNOT_HANDLE, "Parser cannot handle given image: invalid signature!");
            return FALSE;
        }
    }
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_IMAGE_ID, "%s: parser can handle given image!\n", __debug__);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: parsing the image...\n", __debug__);

    /* Create disc */
    self->priv->disc = g_object_new(MIRAGE_TYPE_DISC, NULL);
    mirage_object_set_parent(MIRAGE_OBJECT(self->priv->disc), self);

    nrg_filename = mirage_stream_get_filename(self->priv->nrg_stream);
    mirage_disc_set_filename(self->priv->disc, nrg_filename);

    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: NRG filename: %s\n", __debug__, nrg_filename);

    /* Set CD-ROM as default medium type, will be changed accordingly if there
       is a MTYP block provided */
    mirage_disc_set_medium_type(self->priv->disc, MIRAGE_MEDIUM_CD);

    /* Read descriptor data */
    self->priv->nrg_data = g_malloc(self->priv->nrg_data_length);
    mirage_stream_seek(self->priv->nrg_stream, trailer_offset, G_SEEK_SET, NULL);
    if (mirage_stream_read(self->priv->nrg_stream, self->priv->nrg_data, self->priv->nrg_data_length, NULL) != self->priv->nrg_data_length) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to read descriptor!\n", __debug__);
        g_set_error(error, MIRAGE_ERROR, MIRAGE_ERROR_IMAGE_FILE_ERROR, "Failed to read descriptor!");
        succeeded = FALSE;
        goto end;
    }

    /* Build an index over blocks contained in the parser image */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: building block index...\n", __debug__);
    if(!mirage_parser_nrg_build_block_index(self, error)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to build block index!\n", __debug__);
        succeeded = FALSE;
        goto end;
    }
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: finished building block index\n\n", __debug__);

    /* We'll have to set disc layout start to -150 at some point, so we might
       as well do it here (loading CUEX/CUES session will change this, if ever
       needed) */
    mirage_disc_layout_set_start_sector(self->priv->disc, -150);

    /* Load sessions */
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: loading sessions...\n", __debug__);
    for (gint session_num = 0; ; session_num++) {
        if (mirage_parser_nrg_find_block_entry(self, "CUEX", session_num) || mirage_parser_nrg_find_block_entry(self, "CUES", session_num)) {
            /* CUEX/CUES block: means we need to make new session */
            if (!mirage_parser_nrg_load_session(self, session_num, error)) {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to load session!\n", __debug__);
                succeeded = FALSE;
                goto end;
            }
        } else if (mirage_parser_nrg_find_block_entry(self, "ETN2", session_num) || mirage_parser_nrg_find_block_entry(self, "ETNF", session_num)) {
            /* ETNF/ETN2 block: means we need to make new session */
            if (!mirage_parser_nrg_load_session_tao(self, session_num, error)) {
                MIRAGE_DEBUG(self, MIRAGE_DEBUG_WARNING, "%s: failed to load session!\n", __debug__);
                succeeded = FALSE;
                goto end;
            }
        } else {
            MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: loaded a total of %i sessions.\n", __debug__, session_num);
            break;
        }
    }
    MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: done loading sessions\n\n", __debug__);

    /* Load CD text, medium type etc. */
    if (mirage_parser_nrg_find_block_entry(self, "CDTX", 0)) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: loading CD-TEXT...\n", __debug__);
        if (!mirage_parser_nrg_load_cdtext(self, error)) {
            succeeded = FALSE;
            goto end;
        }
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: finished loading CD-TEXT\n\n", __debug__);
    }

    if (mirage_parser_nrg_find_block_entry(self, "MTYP", 0)) {
        /* MTYP: medium type */
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: determining medium type...\n", __debug__);
        if (!mirage_parser_nrg_load_medium_type(self, error)) {
            succeeded = FALSE;
            goto end;
        }
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: finished determining medium type\n\n", __debug__);
    }

end:
    /* Destroy block index */
    mirage_parser_nrg_destroy_block_index(self);

    /* Return disc */
    if (succeeded) {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: parsing completed successfully\n\n", __debug__);
        return self->priv->disc;
    } else {
        MIRAGE_DEBUG(self, MIRAGE_DEBUG_PARSER, "%s: parsing failed!\n\n", __debug__);
        g_object_unref(self->priv->disc);
        return NULL;
    }
}


/**********************************************************************\
 *                             Object init                            *
\**********************************************************************/
G_DEFINE_DYNAMIC_TYPE(MirageParserNrg, mirage_parser_nrg, MIRAGE_TYPE_PARSER);

void mirage_parser_nrg_type_register (GTypeModule *type_module)
{
    return mirage_parser_nrg_register_type(type_module);
}


static void mirage_parser_nrg_init (MirageParserNrg *self)
{
    self->priv = MIRAGE_PARSER_NRG_GET_PRIVATE(self);

    mirage_parser_generate_info(MIRAGE_PARSER(self),
        "PARSER-NRG",
        "NRG Image Parser",
        1,
        "Nero Burning Rom images (*.nrg)", "application/x-nrg"
    );

    self->priv->nrg_stream = NULL;
    self->priv->nrg_data = NULL;
}

static void mirage_parser_nrg_dispose (GObject *gobject)
{
    MirageParserNrg *self = MIRAGE_PARSER_NRG(gobject);

    if (self->priv->nrg_stream) {
        g_object_unref(self->priv->nrg_stream);
        self->priv->nrg_stream = NULL;
    }

    /* Chain up to the parent class */
    return G_OBJECT_CLASS(mirage_parser_nrg_parent_class)->dispose(gobject);
}


static void mirage_parser_nrg_finalize (GObject *gobject)
{
    MirageParserNrg *self = MIRAGE_PARSER_NRG(gobject);

    g_free(self->priv->nrg_data);

    /* Chain up to the parent class */
    return G_OBJECT_CLASS(mirage_parser_nrg_parent_class)->finalize(gobject);
}

static void mirage_parser_nrg_class_init (MirageParserNrgClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    MirageParserClass *parser_class = MIRAGE_PARSER_CLASS(klass);

    gobject_class->dispose = mirage_parser_nrg_dispose;
    gobject_class->finalize = mirage_parser_nrg_finalize;

    parser_class->load_image = mirage_parser_nrg_load_image;

    /* Register private structure */
    g_type_class_add_private(klass, sizeof(MirageParserNrgPrivate));
}

static void mirage_parser_nrg_class_finalize (MirageParserNrgClass *klass G_GNUC_UNUSED)
{
}
