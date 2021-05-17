/*
 * Copyright 2020-2021 Niklas Ekström
 */

#ifndef PROTO_A314_H
#define PROTO_A314_H

extern struct Library *A314Base;

ULONG __TranslateAddressA314(__reg("a6") void *, __reg("a0") void *)="\tjsr\t-42(a6)";
#define TranslateAddressA314(address) __TranslateAddressA314(A314Base, address)

#endif
