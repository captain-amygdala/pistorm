#include "../config_file/config_file.h"

enum base_platforms {
    PLATFORM_NONE,
    PLATFORM_AMIGA,
    PLATFORM_MAC,
    PLATFORM_X68000,
    PLATFORM_NUM,
};

struct platform_config *make_platform_config(char *name, char *subsys);
