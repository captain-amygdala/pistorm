void piscsi_init();
void piscsi_map_drive(char *filename, uint8_t index);

void handle_piscsi_write(uint32_t addr, uint32_t val, uint8_t type);
uint32_t handle_piscsi_read(uint32_t addr, uint8_t type);

void piscsi_block_op(uint8_t type, uint8_t num, uint32_t dest, uint32_t len);

struct piscsi_dev {
    uint32_t c;
    uint16_t h, s;
    uint64_t fs;
    int32_t fd;
};
