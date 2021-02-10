#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>
#include "hunk-reloc.h"

#define DEBUG(...)
//#define DEBUG printf

#define READLW(a, b) fread(&a, 4, 1, b); a = be32toh(a);
#define READW(a, b) fread(&a, 2, 1, b); a = be16toh(a);

uint32_t lw;

char *hunk_id_name(uint32_t index) {
    switch (index) {
        case HUNKTYPE_HEADER:
            return "HUNK_HEADER";
        case HUNKTYPE_CODE:
            return "HUNK_CODE";
        case HUNKTYPE_HUNK_RELOC32:
            return "HUNK_RELOC32";
        case HUNKTYPE_SYMBOL:
            return "HUNK_SYMBOL";
        case HUNKTYPE_BSS:
            return "HUNK_BSS";
        case HUNKTYPE_DATA:
            return "HUNK_DATA";
        case HUNKTYPE_END:
            return "HUNK_END";
        default:
            return "UNKNOWN HUNK TYPE";
    }
}

int process_hunk(uint32_t index, struct hunk_info *info, FILE *f, struct hunk_reloc *r) {
    if (!f)
        return -1;
    
    uint32_t discard = 0, cur_hunk = 0, offs32 = 0;
    
    switch (index) {
        case HUNKTYPE_HEADER:
            DEBUG("Processing hunk header.\n");
            do {
                READLW(discard, f);
                if (discard) {
                    info->libnames[info->num_libs] = malloc(discard * 4);
                    fread(info->libnames[info->num_libs], discard, 4, f);
                    info->num_libs++;
                }
            } while (discard);
            
            READLW(info->table_size, f);
            DEBUG("Table size: %d\n", info->table_size);
            READLW(info->first_hunk, f);
            READLW(info->last_hunk, f);
            info->num_hunks = (info->last_hunk - info->first_hunk) + 1;
            DEBUG("First: %d Last: %d Num: %d\n", info->first_hunk, info->last_hunk, info->num_hunks);
            info->hunk_sizes = malloc(info->num_hunks * 4);
            info->hunk_offsets = malloc(info->num_hunks * 4);
            for (uint32_t i = 0; i < info->table_size; i++) {
                READLW(info->hunk_sizes[i], f);
                DEBUG("Hunk %d: %d (%.8X)\n", i, info->hunk_sizes[i] * 4, info->hunk_sizes[i] * 4);
            }
            return 0;
            break;
        case HUNKTYPE_CODE:
            DEBUG("Hunk %d: CODE.\n", info->current_hunk);
            READLW(discard, f);
            info->hunk_offsets[info->current_hunk] = ftell(f);
            DEBUG("Code hunk size: %d (%.8X)\n", discard * 4, discard * 4);
            fseek(f, discard * 4, SEEK_CUR);
            return 0;
            break;
        case HUNKTYPE_HUNK_RELOC32:
            DEBUG("Hunk %d: RELOC32.\n", info->current_hunk);
            DEBUG("Processing Reloc32 hunk.\n");
            do {
                READLW(discard, f);
                if (discard) {
                    READLW(cur_hunk, f);
                    DEBUG("Relocating %d offsets pointing to hunk %d.\n", discard, cur_hunk);
                    for(uint32_t i = 0; i < discard; i++) {
                        READLW(offs32, f);
                        DEBUG("#%d: @%.8X in hunk %d\n", i + 1, offs32, cur_hunk);
                        r[info->reloc_hunks].offset = offs32;
                        r[info->reloc_hunks].src_hunk = info->current_hunk;
                        r[info->reloc_hunks].target_hunk = cur_hunk;
                        info->reloc_hunks++;
                    }
                }
            } while(discard);
            return 0;
            break;
        case HUNKTYPE_SYMBOL:
            DEBUG("Hunk %d: SYMBOL.\n", info->current_hunk);
            DEBUG("Processing Symbol hunk.\n");
            READLW(discard, f);
            do {
                if (discard) {
                    char sstr[256];
                    memset(sstr, 0x00, 256);
                    fread(sstr, discard, 4, f);
                    READLW(discard, f);
                    DEBUG("Symbol: %s - %.8X\n", sstr, discard);
                }
                READLW(discard, f);
            } while (discard);
            return 0;
            break;
        case HUNKTYPE_BSS:
            DEBUG("Hunk %d: BSS.\n", info->current_hunk);
            READLW(discard, f);
            info->hunk_offsets[info->current_hunk] = ftell(f);
            DEBUG("Skipping BSS hunk. Size: %d\n", discard * 4);
            return 0;
        case HUNKTYPE_DATA:
            DEBUG("Hunk %d: DATA.\n", info->current_hunk);
            READLW(discard, f);
            info->hunk_offsets[info->current_hunk] = ftell(f);
            DEBUG("Skipping data hunk. Size: %d.\n", discard * 4);
            fseek(f, discard * 4, SEEK_CUR);
            return 0;
            break;
        case HUNKTYPE_END:
            DEBUG("END: Ending hunk %d.\n", info->current_hunk);
            info->current_hunk++;
            return 0;
            break;
        default:
            DEBUG("Unknown hunk type %.8X! Can't process!\n", index);
            break;
    }

    return -1;
}

void reloc_hunk(struct hunk_reloc *h, uint8_t *buf, struct hunk_info *i) {
    uint32_t rel = i->hunk_offsets[h->target_hunk];
    uint32_t *src_ptr = (uint32_t *)(&buf[i->hunk_offsets[h->src_hunk] + h->offset]);

    uint32_t src = be32toh(*src_ptr);
    uint32_t dst = src + i->base_offset + rel;
    DEBUG("%.8X -> %.8X\n", src, dst);
    *src_ptr = htobe32(dst);
}

void process_hunks(FILE *in, struct hunk_info *h_info, struct hunk_reloc *r) {
    READLW(lw, in);
    DEBUG("Hunk ID: %.8X (%s)\n", lw, hunk_id_name(lw));

    while(!feof(in) && process_hunk(lw, h_info, in, r) != -1) {
        READLW(lw, in);
        if (feof(in)) goto end_parse;
        DEBUG("Hunk ID: %.8X (%s)\n", lw, hunk_id_name(lw));
        DEBUG("File pos: %.8lX\n", ftell(in));
    }
    end_parse:;
    DEBUG("Done processing hunks.\n");
}

void reloc_hunks(struct hunk_reloc *r, uint8_t *buf, struct hunk_info *h_info) {
    for(uint32_t i = 0; i < h_info->reloc_hunks; i++) {
        reloc_hunk(&r[i], buf, h_info);
        DEBUG("Relocating offset %d.\n", i);
    }
    DEBUG("Done relocating offsets.\n");
}
