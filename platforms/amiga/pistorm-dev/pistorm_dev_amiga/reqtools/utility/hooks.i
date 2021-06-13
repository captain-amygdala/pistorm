	IFND UTILITY_HOOKS_I
UTILITY_HOOKS_I SET 1
**
**   $Filename: utility/tagitem.i $
**   $Release: 1.0 $
**
**   Clone of 2.0 include file 'utility/hools.i'
**

   IFND EXEC_TYPES_I
   INCLUDE "exec/types.i"
   ENDC

   IFND EXEC_NODES_I
   INCLUDE "exec/nodes.i"
   ENDC

   STRUCTURE HOOK,MLN_SIZE
      APTR h_Entry
      APTR h_SubEntry
      APTR h_Data
      LABEL h_SIZEOF

   ENDC ; UTILITY_HOOKS_I
