#include "../platforms.h"
#include <stdlib.h>
#include <string.h>

int handle_register_read_dummy(unsigned int addr, unsigned char type, unsigned int *val);
int handle_register_write_dummy(unsigned int addr, unsigned int value, unsigned char type);

int setup_platform_dummy(struct emulator_config *cfg) {
    if (cfg) {}
    return 0;
}

void create_platform_dummy(struct platform_config *cfg, char *subsys) {
    cfg->custom_read = NULL;
    cfg->custom_write = NULL;
    cfg->register_read = handle_register_read_dummy;
    cfg->register_write = handle_register_write_dummy;
    cfg->platform_initial_setup = setup_platform_dummy;

    if (subsys) {
        cfg->subsys = malloc(strlen(subsys) + 1);
        strcpy(cfg->subsys, subsys);
    }
}
