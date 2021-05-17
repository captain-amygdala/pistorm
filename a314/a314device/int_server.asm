*
* Copyright 2020-2021 Niklas Ekström
*

	XDEF	_IntServer
	CODE

SIGB_INT	equ	14
SIGF_INT	equ	(1 << SIGB_INT)

		; a1 points to interrupt_data
_IntServer:	move.l	4(a1),a5	; interrupt_data.ca

		move.b	0(a5),d0	; A_EVENTS
		and.b	1(a5),d0	; A_ENABLE
		beq.s	should_not_signal

		move.b 	#0,1(a5)

		move.l	$4.w,a6
		move.l	#SIGF_INT,d0
		move.l	0(a1),a1	; interrupt_data.task
		jsr	-324(a6)	; Signal()

should_not_signal:
		moveq	#0,d0
		rts
