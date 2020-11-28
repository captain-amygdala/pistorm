	XDEF	_IntServer
	CODE

COM_AREA	equ	$e90000

SIGB_INT	equ	14
SIGF_INT	equ	(1 << SIGB_INT)

		; a1 points to driver task
_IntServer:	lea.l	COM_AREA,a5

		move.b	0(a5),d0	; A_EVENTS
		and.b	1(a5),d0	; A_ENABLE
		beq.s	should_not_signal

		move.b 	#0,1(a5)

		move.l	$4.w,a6
		move.l	#SIGF_INT,d0
		; a1 = pointer to driver task
		jsr	-324(a6)	; Signal()

should_not_signal:
		moveq	#0,d0
		rts
