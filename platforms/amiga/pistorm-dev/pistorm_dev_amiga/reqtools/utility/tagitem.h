#ifndef UTILITY_TAGITEM_H
#define UTILITY_TAGITEM_H
/*
**   $Filename: utility/tagitem.i $
**   $Release: 1.0 $
**
**   Clone of 2.0 include file 'utility/tagitem.h'
*/

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif	/* EXEC_TYPES_H */

typedef ULONG Tag;

struct TagItem {
   Tag ti_Tag;
   ULONG ti_Data;
   };

#define TAG_DONE		 0L
#define TAG_END			 0L
#define TAG_IGNORE		 1L
#define TAG_MORE		 2L
#define TAG_SKIP		 3L
#define TAG_USER		 0x80000000
#define TAGFILTER_AND		 0
#define TAGFILTER_NOT		 1

#endif /* UTILITY_TAGITEM_H */
