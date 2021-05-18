		dc.l	0

		movem.l	a0-a6,-(a7)
		lsl.l	#2,d1
		move.l	d1,a0
		bsr	_start
		movem.l	(a7)+,a0-a6
		jmp	(a6)
