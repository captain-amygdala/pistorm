#ifndef UTILITY_HOOKS_H
#define UTILITY_HOOKS_H
/*
**   $Filename: utility/tagitem.i $
**   $Release: 1.0 $
**
**   Clone of 2.0 include file 'utility/hooks.h'
*/

#ifndef EXEC_TYPES_H
#include "exec/types.h"
#endif /* EXEC_TYPES_H */

#ifndef EXEC_NODES_H
#include "exec/nodes.h"
#endif /* EXEC_NODES_H */

struct Hook {
   struct MinNode h_MinNode;
   ULONG (*h_Entry)();
   ULONG (*h_SubEntry)();
   APTR h_Data;
   };

#endif /* UTILITY_HOOKS_H */
