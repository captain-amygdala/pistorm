#ifndef _HUNK_RELOC_H
#define _HUNK_RELOC_H

struct hunk_reloc {
    uint32_t src_hunk;
    uint32_t target_hunk;
    uint32_t offset;
};

struct hunk_info {
    uint16_t current_hunk;
    uint16_t num_libs;
    uint8_t *libnames[256];
    uint32_t table_size, byte_size, alloc_size;
    uint32_t base_offset;
    uint32_t first_hunk, last_hunk, num_hunks;
    uint32_t reloc_hunks;
    uint32_t *hunk_offsets;
    uint32_t *hunk_sizes;
};

enum hunk_types {
    HUNKTYPE_CODE = 0x3E9,
    HUNKTYPE_DATA = 0x3EA,
    HUNKTYPE_BSS = 0x3EB,
    HUNKTYPE_HUNK_RELOC32 = 0x3EC,
    HUNKTYPE_SYMBOL = 0x3F0,
    HUNKTYPE_END = 0x3F2,
    HUNKTYPE_HEADER = 0x3F3,
};

int process_hunk(uint32_t index, struct hunk_info *info, FILE *f, struct hunk_reloc *r);
int load_lseg(int fd, uint8_t **buf_p, struct hunk_info *i, struct hunk_reloc *relocs, uint32_t block_size);

void reloc_hunk(struct hunk_reloc *h, uint8_t *buf, struct hunk_info *i);
void process_hunks(FILE *in, struct hunk_info *h_info, struct hunk_reloc *r, uint32_t offset);
void reloc_hunks(struct hunk_reloc *r, uint8_t *buf, struct hunk_info *h_info);

#endif /* _HUNK_RELOC_H */
