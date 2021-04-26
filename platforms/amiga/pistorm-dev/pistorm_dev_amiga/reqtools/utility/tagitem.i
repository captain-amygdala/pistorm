	IFND UTILITY_TAGITEM_I
UTILITY_TAGITEM_I SET	1
**
**   $Filename: utility/tagitem.i $
**   $Release: 1.0 $
**
**   Clone of 2.0 include file 'utility/tagitem.i'
**

   STRUCTURE TagItem,0
      ULONG ti_Tag
      ULONG ti_Data
      LABEL ti_SIZEOF

TAG_END		equ		 0
TAG_DONE	equ		 0
TAG_IGNORE	equ		 1
TAG_MORE	equ		 2
TAG_SKIP	equ		 3
TAG_USER	equ		 $80000000

   ENDC ; UTILITY_TAGITEM_I
