/*
 * Copyright 2020-2021 Niklas Ekström
 */

#include <exec/types.h>

extern ULONG translate_address_a314(__reg("a0") void *address);
extern BOOL fix_memory();
