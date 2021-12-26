; Commander Keen 4 Tandy Version Source Code
; Copyright (C) 2021 Frenkel Smeijers
;
; This file is primarily based on:
; Reconstructed Commander Keen 4-6 Source Code
; Copyright (C) 2021 K1n9_Duk3
;
; This file is primarily based on:
; Catacomb 3-D Source Code
; Copyright (C) 1993-2014 Flat Rock Software
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License along
; with this program; if not, write to the Free Software Foundation, Inc.,
; 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

;=================================
;
; TGA view manager routines
;
;=================================

;============================================================================
;
; All of these routines draw into a floating virtual screen segment in main
; memory.  bufferofs points to the origin of the drawing page in screenseg.
; The routines that write out words must take into account buffer wrapping
; and not write a word at 0xffff (which causes an exception on 386s).
;
; The direction flag should be clear
;
;============================================================================

DATASEG

plotpixels	db	11110000b,00001111b
colorbyte	db	0,11h,22h,33h,44h,55h,66h,77h,88h,99h,0aah,0bbh,0cch,0ddh,0eeh,0ffh
colorword	dw	0,1111h,2222h,3333h,4444h,5555h,6666h,7777h,8888h,9999h,0aaaah,0bbbbh,0cccch,0ddddh,0eeeeh,0ffffh

CODESEG

;============================================================================
;
; VW_Plot (int x,y,color)
;
;============================================================================


PROC	VW_Plot x:WORD, y:WORD, color:WORD
PUBLIC	VW_Plot
USES	SI,DI

	mov	es,[screenseg]

	mov	di,[bufferofs]
	mov	bx,[y]
	shl	bx,1
	add	di,[ylookup+bx]
	mov	bx,[x]
	mov	ax,bx
	shr	ax,1
	shr	ax,1
	add	di,ax				; di = byte on screen

	and	bx,1
	mov	ah,[plotpixels+bx]
	mov	bx,[color]
	mov	cl,[colorbyte+bx]
	and	cl,ah
	not	ah

	mov	al,[es:di]
	and	al,ah				; mask off other pixels
	or	al,cl
	stosb

	ret

ENDP


;============================================================================
;
; VW_Vlin (int yl,yh,x,color)
;
;============================================================================

PROC	VW_Vlin yl:WORD, yh:WORD, x:WORD, color:WORD
PUBLIC	VW_Vlin
USES	SI,DI

	mov	es,[screenseg]

	mov	di,[bufferofs]
	mov	bx,[yl]
	shl	bx,1
	add	di,[ylookup+bx]
	mov	bx,[x]
	mov	ax,bx
	shr	ax,1
	add	di,ax				; di = byte on screen

	and	bx,1
	mov	ah,[plotpixels+bx]
	mov	bx,[color]
	mov	bl,[colorbyte+bx]
	and	bl,ah
	not	ah

	mov	cx,[yh]
	sub	cx,[yl]
	inc	cx					;number of pixels to plot

	mov	dx,[linewidth]

@@plot:
	mov	al,[es:di]
	and	al,ah				; mask off other pixels
	or	al,bl
	mov [es:di],al
	add	di,dx
	loop	@@plot

	ret

	ret

ENDP


;============================================================================


;===================
;
; VW_DrawTile8
;
; xcoord in bytes (8 pixels), ycoord in pixels
; All Tile8s are in one grseg, so an offset is calculated inside it
;
; DONE
;
;===================

PROC	VW_DrawTile8	xcoord:WORD, ycoord:WORD, tile:WORD
PUBLIC	VW_DrawTile8
USES	SI,DI

	mov	es,[screenseg]

	mov	di,[bufferofs]
	add	di,[xcoord]
	mov	bx,[ycoord]
	shl	bx,1
	add	di,[ylookup+bx]

	mov	bx,[linewidth]
	sub	bx,4

	mov	si,[tile]
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1

	mov	ds,[grsegs+STARTTILE8*2] ; segment for all tile8s

;
; start drawing
;

REPT	7
	movsb						;no word moves because of segment wrapping
	movsb
	movsb
	movsb
	add	di,bx
ENDM
	movsb
	movsb
	movsb
	movsb

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

ENDP


;============================================================================
;
; VW_MaskBlock
;
; Draws a masked block shape to the screen.  bufferofs is NOT accounted for.
; The mask comes first, then the data.  Seperate unwound routines are used
; to speed drawing.
;
; DONE
;
;============================================================================

DATASEG

UNWOUNDMASKS	=	20


maskroutines	dw	mask0,mask0
				dw	mask1,mask1
				dw	mask2E,mask2O
				dw	mask3E,mask3O
				dw	mask4E,mask4O
				dw	mask5E,mask5O
				dw	mask6E,mask6O
				dw	mask7E,mask7O
				dw	mask8E,mask8O
				dw	mask9E,mask9O
				dw	mask10E,mask10O
				dw	mask11E,mask11O
				dw	mask12E,mask12O
				dw	mask13E,mask13O
				dw	mask14E,mask14O
				dw	mask15E,mask15O
				dw	mask16E,mask16O
				dw	mask17E,mask17O
				dw	mask18E,mask18O
				dw	mask19E,mask19O
				dw	mask20E,mask20O


CODESEG

PROC	VW_MaskBlock	segm:WORD, ofs:WORD, dest:WORD, wide:WORD, height:WORD, planesize:WORD
PUBLIC	VW_MaskBlock
USES	SI,DI

	mov	es,[screenseg]

	mov	di,[wide]
	mov	dx,[linewidth]
	sub	dx,di					;dx = delta to start of next line

	mov	bx,[planesize]			; si+bx = data location

	cmp	di,UNWOUNDMASKS
	jbe	@@unwoundroutine

;==============
;
; General purpose masked block drawing.  This could be optimised into
; four routines to use words, but few play loop sprites should be this big!
;
;==============

	mov	[ss:linedelta],dx
	mov	ds,[segm]
	mov	si,[ofs]
	mov	di,[dest]
	mov	dx,[height]				;scan lines to draw

@@lineloopgen:
	mov	cx,[wide]
@@byteloop:
	mov	al,[es:di]
	and	al,[si]
	or	al,[bx+si]
	inc	si
	stosb
	loop	@@byteloop

	add	di,[ss:linedelta]
	dec	dx
	jnz	@@lineloopgen

mask0:
	mov	ax,ss
	mov	ds,ax
	ret							;width of 0 = no drawing


;=================
;
; use the unwound routines
;
;=================

@@unwoundroutine:
	mov	cx,[dest]
	shr	cx,1
	rcl	di,1					;shift a 1 in if destination is odd
	shl	di,1
	mov	ax,[maskroutines+di]	;call the right routine

	mov	ds,[segm]
	mov	si,[ofs]
	mov	di,[dest]
	mov	cx,[height]				;scan lines to draw

	jmp ax						;draw it

;=================
;
; Horizontally unwound routines to draw certain masked blocks faster
;
;=================

MACRO	MASKBYTE
	mov	al,[es:di]
	and	al,[si]
	or	al,[bx+si]
	inc	si
	stosb
ENDM

MACRO	MASKWORD
	mov	ax,[es:di]
	and	ax,[si]
	or	ax,[bx+si]
	inc	si
	inc	si
	stosw
ENDM

MACRO	SPRITELOOP	addr
	add	di,dx
	loop	addr
	mov	ax,ss
	mov	ds,ax
	ret
ENDM


EVEN
mask1:
	MASKBYTE
	SPRITELOOP	mask1

EVEN
mask2E:
	MASKWORD
	SPRITELOOP	mask2E

EVEN
mask2O:
	MASKBYTE
	MASKBYTE
	SPRITELOOP	mask2O

EVEN
mask3E:
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask3E

EVEN
mask3O:
	MASKBYTE
	MASKWORD
	SPRITELOOP	mask3O

EVEN
mask4E:
	MASKWORD
	MASKWORD
	SPRITELOOP	mask4E

EVEN
mask4O:
	MASKBYTE
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask4O

EVEN
mask5E:
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask5E

EVEN
mask5O:
	MASKBYTE
	MASKWORD
	MASKWORD
	SPRITELOOP	mask5O

EVEN
mask6E:
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask6E

EVEN
mask6O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask6O

EVEN
mask7E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask7E

EVEN
mask7O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask7O

EVEN
mask8E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask8E

EVEN
mask8O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask8O

EVEN
mask9E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask9E

EVEN
mask9O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask9O

EVEN
mask10E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask10E

EVEN
mask10O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask10O

EVEN
mask11E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask11E

EVEN
mask11O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask11O

EVEN
mask12E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask12E

EVEN
mask12O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask12O

EVEN
mask13E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask13E

EVEN
mask13O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask13O

EVEN
mask14E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask14E

EVEN
mask14O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask14O

EVEN
mask15E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask15E

EVEN
mask15O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask15O

EVEN
mask16E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask16E

EVEN
mask16O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask16O

EVEN
mask17E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask17E

EVEN
mask17O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask17O

EVEN
mask18E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask18E

EVEN
mask18O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask18O

EVEN
mask19E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask19E

EVEN
mask19O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask19O

EVEN
mask20E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask20E

EVEN
mask20O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask20O


ENDP


;============================================================================
;
; VW_MaskBlockClipped
;
;============================================================================

PROC	VW_MaskBlockClipped	segm:WORD, ofs:WORD, dest:WORD, wide:WORD, height:WORD, planesize:WORD, spritewidthdelta:WORD
PUBLIC	VW_MaskBlockClipped
USES	SI,DI

	mov	es,[screenseg]

	mov	dx,[linewidth]
	sub	dx,[wide]
	mov	[ss:linedelta],dx

	mov	bx,[planesize]

	mov	ds,[segm]
	mov	si,[ofs]
	mov	di,[dest]
	mov	dx,[height]

@@yloop:
	mov	cx,[wide]
@@xloop:
	mov	al,[es:di]
	and	al,[si]
	or	al,[bx+si]
	inc	si
	stosb
	loop	@@xloop

	add si,[spritewidthdelta]
	add	di,[ss:linedelta]
	dec	dx
	jnz	@@yloop

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment
	ret
ENDP


;============================================================================
;
; VW_ScreenToScreen
;
; Basic block copy routine.  Copies one block of screen memory to another,
; bufferofs is NOT accounted for.
;
; DONE
;
;============================================================================

PROC	VW_ScreenToScreen	source:WORD, dest:WORD, wide:WORD, height:WORD
PUBLIC	VW_ScreenToScreen
USES	SI,DI

	mov	bx,[linewidth]
	sub	bx,[wide]

	mov	ax,[screenseg]
	mov	es,ax
	mov	ds,ax

	mov	si,[source]
	mov	di,[dest]				;start at same place in all planes
	mov	dx,[height]				;scan lines to draw
	mov	ax,[wide]
;
; if the width, source, and dest are all even, use word moves
; This is allways the case in the TGA refresh
;
	test	ax,1
	jnz	@@bytelineloop
	test	si,1
	jnz	@@bytelineloop
	test	di,1
	jnz	@@bytelineloop

	shr	ax,1
@@wordlineloop:
	mov	cx,ax
	rep	movsw
	add	si,bx
	add	di,bx

	dec	dx
	jnz	@@wordlineloop

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

@@bytelineloop:
	mov	cx,ax
	rep	movsb
	add	si,bx
	add	di,bx

	dec	dx
	jnz	@@bytelineloop

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

ENDP


;============================================================================
;
; VW_MemToScreen
;
; Basic block drawing routine. Takes a block shape at segment pointer source
; of width by height data, and draws it to dest in the virtual screen,
; based on linewidth.  bufferofs is NOT accounted for.
; There are four drawing routines to provide the best optimized code while
; accounting for odd segment wrappings due to the floating screens.
;
; DONE
;
;============================================================================

DATASEG

memtoscreentable	dw	eventoeven,eventoodd,oddtoeven,oddtoodd

CODESEG


PROC	VW_MemToScreen	source:WORD, dest:WORD, wide:WORD, height:WORD
PUBLIC	VW_MemToScreen
USES	SI,DI

	mov	es,[screenseg]

	mov	bx,[linewidth]
	sub	bx,[wide]

	mov	ds,[source]

	xor	si,si					;block is segment aligned

	xor	di,di
	shr	[wide],1				;change wide to words, and see if carry is set
	rcl	di,1					;1 if wide is odd
	mov	ax,[dest]
	shr	ax,1
	rcl	di,1					;shift a 1 in if destination is odd
	shl	di,1					;to index into a word width table
	mov	dx,[height]				;scan lines to draw
	mov	ax,[wide]
	jmp	[ss:memtoscreentable+di]	;call the right routine

;==============
;
; Copy an even width block to an even destination address
;
;==============

eventoeven:
	mov	di,[dest]				;start at same place in all planes
EVEN
@@lineloopEE:
	mov	cx,ax
	rep	movsw
	add	di,bx
	dec	dx
	jnz	@@lineloopEE

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

;==============
;
; Copy an odd width block to an even video address
;
;==============

oddtoeven:
	mov	di,[dest]				;start at same place in all planes
EVEN
@@lineloopOE:
	mov	cx,ax
	rep	movsw
	movsb						;copy the last byte
	add	di,bx
	dec	dx
	jnz	@@lineloopOE

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

;==============
;
; Copy an even width block to an odd video address
;
;==============

eventoodd:
	mov	di,[dest]				;start at same place in all planes
	dec	ax						;one word has to be handled seperately
EVEN
@@lineloopEO:
	movsb
	mov	cx,ax
	rep	movsw
	movsb
	add	di,bx
	dec	dx
	jnz	@@lineloopEO

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

;==============
;
; Copy an odd width block to an odd video address
;
;==============

oddtoodd:
	mov	di,[dest]				;start at same place in all planes
EVEN
@@lineloopOO:
	movsb
	mov	cx,ax
	rep	movsw
	add	di,bx
	dec	dx
	jnz	@@lineloopOO

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment
	ret


ENDP

;===========================================================================
;
; VW_ScreenToMem
;
; Copies a block of video memory to main memory, in order from planes 0-3.
; This could be optimized along the lines of VW_MemToScreen to take advantage
; of word copies, but this is an infrequently called routine.
;
; DONE
;
;===========================================================================

PROC	VW_ScreenToMem	source:WORD, dest:WORD, wide:WORD, height:WORD
PUBLIC	VW_ScreenToMem
USES	SI,DI

	mov	es,[dest]

	mov	bx,[linewidth]
	sub	bx,[wide]

	mov	ds,[screenseg]

	xor	di,di

	mov	si,[source]
	mov	dx,[height]				;scan lines to draw

@@lineloop:
	mov	cx,[wide]
	rep	movsb

	add	si,bx

	dec	dx
	jnz	@@lineloop

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

ENDP


;===========================================================================
;
;                    MISC TGA ROUTINES
;
;===========================================================================

;==============
;
; VW_SetScreen
;
; DONE
;
;==============

PROC	VW_SetScreen  crtc:WORD
PUBLIC	VW_SetScreen

;
; for some reason, my XT's EGA card doesn't like word outs to the CRTC
; index...
;
	cli

	mov	cx,[crtc]
	mov	dx,CRTC_INDEX
	mov	al,0ch		;start address high register
	out	dx,al
	inc	dx
	mov	al,ch
	out	dx,al
	dec	dx
	mov	al,0dh		;start address low register
	out	dx,al
	mov	al,cl
	inc	dx
	out	dx,al

	sti

	ret

ENDP


if NUMFONT+NUMFONTM

;===========================================================================
;
; GENERAL FONT DRAWING ROUTINES
;
;===========================================================================

DATASEG

px	dw	?					; proportional character drawing coordinates
py	dw	?
pdrawmode	db	11000b		; 8 = OR, 24 = XOR, put in GC_DATAROTATE
fontcolor	db	15		;0-15 mapmask value

PUBLIC	px,py,pdrawmode,fontcolor

;
; offsets in font structure
;
pcharheight	=	0		;lines high
charloc		=	2		;pointers to every character
charwidth	=	514		;every character's width in pixels


propchar	dw	?			; the character number to shift
stringptr	dw	?,?

fontcolormask	dw	?			; font color expands into this

BUFFWIDTH	=	105
BUFFHEIGHT	=   32			; must be twice as high as font for masked fonts

databuffer	db	BUFFWIDTH*BUFFHEIGHT dup (?)

bufferwidth	dw	?						; bytes with valid info / line
bufferheight dw	?						; number of lines currently used

bufferbyte	dw	?
bufferbit	dw	?
PUBLIC	bufferwidth,bufferheight,bufferbyte,bufferbit

screenspot	dw	?						; where the buffer is going

bufferextra	dw	?						; add at end of a line copy
screenextra	dw	?

CODESEG

;======================
;
; Macros to table shift a byte of font
;
;======================

MACRO	SHIFTNOXOR
	mov	al,[es:bx]		; source
	xor	ah,ah
	shl	ax,1
	mov	si,ax
	mov	ax,[bp+si]		; table shift into two bytes
	or	[di],al			; or with first byte
	inc	di
	mov	[di],ah			; replace next byte
	inc	bx				; next source byte
ENDM


;=======================
;
; VWL_XORBuffer
;
; Pass buffer start in SI (somewhere in databuffer)
; Draws the buffer to the screen buffer
;
;========================

PROC	VWL_XORBuffer	NEAR
USES	BP
	mov	bl,[fontcolor]
	xor	bh,bh
	shl	bx,1
	mov	ax,[colorword+bx]
	mov	[fontcolormask],ax

	mov	es,[screenseg]
	mov	di,[screenspot]

	mov	bx,[bufferwidth]		;calculate offsets for end of each line
	mov	[bufferwidth],bx

	or	bx,bx
	jnz	@@isthere
	ret							;nothing to draw

@@isthere:
	test	bx,1
	jnz	@@odd
	jmp	@@even
;
; clear the last byte so word draws can be used
;
@@odd:
	mov	al,0
line	=	0
REPT	BUFFHEIGHT
	mov	[BYTE databuffer+BUFFWIDTH*line+bx],al
line	=	line+1
ENDM

	inc	bx
@@even:
	mov	ax,[linewidth]
	sub	ax,bx
	mov	[screenextra],ax
	mov	ax,BUFFWIDTH
	sub	ax,bx
	mov	[bufferextra],ax
	mov	dx,bx

	mov	bx,[bufferheight]		;lines to copy
	mov	bp,[fontcolormask]
@@lineloop:
	mov	cx,dx
@@byteloop:
	lodsb						;get a word from the buffer
	and	ax,bp
	xor	[es:di],al				;draw it
	inc	di
	loop	@@byteloop

	add	si,[bufferextra]
	add	di,[screenextra]

	dec	bx
	jnz	@@lineloop

	ret
ENDP


DATASEG

;============================================================================
;
; NON MASKED FONT DRAWING ROUTINES
;
;============================================================================

if numfont

DATASEG

shiftdrawtable	dw      0,shift1wide,shift2wide,shift3wide,shift4wide
				dw		shift5wide,shift6wide

CODESEG

;==================
;
; ShiftPropChar
;
; Call with BX = character number (0-255)
; Draws one character to the buffer at bufferbyte/bufferbit, and adjusts
; them to the new position
;
;==================

PROC	ShiftPropChar	NEAR

	mov	si,[fontnumber]
	shl	si,1
	mov	es,[grsegs+STARTFONT*2+si]	;segment of font to use

;
; find character location, width, and height
;
	mov	si,[es:charwidth+bx]
	and	si,0ffh					;SI hold width in pixels
	shl	bx,1
	mov	bx,[es:charloc+bx]		;BX holds pointer to character data

;
; look up which shift table to use, based on bufferbit
;
	mov	di,[bufferbit]
	shl	di,1
	mov	bp,[shifttabletable+di]	;BP holds pointer to shift table

	mov	di,OFFSET databuffer
	add	di,[bufferbyte]			;DI holds pointer to buffer

	mov	cx,[bufferbit]
	add	cx,si					;add four times because pixel == four bits
	add	cx,si
	add	cx,si
	add	cx,si					;new bit position
	mov	ax,cx
	and	ax,7
	mov	[bufferbit],ax			;new bit position
	mov	ax,cx
	shr	ax,1
	shr	ax,1
	shr	ax,1
	add	[bufferbyte],ax			;new byte position

	add	si,1
	shr	si,1					;bytes the character is wide
	shl	si,1                    ;*2 to look up in shiftdrawtable

	mov	cx,[es:pcharheight]
	mov	dx,BUFFWIDTH
	jmp	[ss:shiftdrawtable+si]	;procedure to draw this width

;
; one byte character
;
shift1wide:
	dec	dx
EVEN
@@loop1:
	SHIFTNOXOR
	add	di,dx			; next line in buffer

	loop	@@loop1

	ret

;
; two byte character
;
shift2wide:
	dec	dx
	dec	dx
EVEN
@@loop2:
	SHIFTNOXOR
	SHIFTNOXOR
	add	di,dx			; next line in buffer

	loop	@@loop2

	ret

;
; three byte character
;
shift3wide:
	sub	dx,3
EVEN
@@loop3:
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	add	di,dx			; next line in buffer

	loop	@@loop3

	ret


;
; four byte character
;
shift4wide:
	sub	dx,4
EVEN
@@loop4:
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	add	di,dx			; next line in buffer

	loop	@@loop4

	ret


;
; five byte character
;
shift5wide:
	sub	dx,5
EVEN
@@loop5:
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	add	di,dx			; next line in buffer

	loop	@@loop5

	ret

;
; six byte character
;
shift6wide:
	sub	dx,6
EVEN
@@loop6:
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	add	di,dx			; next line in buffer

	loop	@@loop6

	ret



ENDP

;============================================================================

;==================
;
; VW_DrawPropString
;
; Draws a C string of characters at px/py and advances px
;
;==================

CODESEG

PROC	VW_DrawPropString	string:DWORD
PUBLIC	VW_DrawPropString
USES	SI,DI

;
; proportional spaceing, which clears the buffer ahead of it, so only
; clear the first collumn
;
	mov	al,0
line	=	0
REPT	BUFFHEIGHT
	mov	[BYTE databuffer+BUFFWIDTH*line],al
line	=	line+1
ENDM

;
; shift the characters into the buffer
;
@@shiftchars:
	mov	ax,[px]
	and	ax,1
	shl	ax,1
	shl	ax,1			;one pixel == four bits
	mov	[bufferbit],ax
	mov	[bufferbyte],0

	mov	ax,[WORD string]
	mov	[stringptr],ax
	mov	ax,[WORD string+2]
	mov	[stringptr+2],ax

@@shiftone:
	mov	es,[stringptr+2]
	mov	bx,[stringptr]
	inc	[stringptr]
	mov	bx,[es:bx]
	xor	bh,bh
	or	bl,bl
	jz	@@allshifted
	call	ShiftPropChar
	jmp	@@shiftone

@@allshifted:
;
; calculate position to draw buffer on screen
;
	mov	bx,[py]
	shl	bx,1
	mov	di,[ylookup+bx]
	add	di,[bufferofs]
	add	di,[panadjust]

	mov	ax,[px]
	shr	ax,1		;x location in bytes
	add	di,ax
	mov	[screenspot],di

;
; advance px
;
	mov	ax,[bufferbyte]
	shl	ax,1
	mov	bx,[bufferbit]
	shr	bx,1
	shr	bx,1			;four bits == one pixel
	or	ax,bx
	add	[px],ax

;
; draw it
;
	mov	ax,[bufferbyte]
	test	[bufferbit],7
	jz	@@go
	inc	ax				;so the partial byte also gets drawn
@@go:
	mov	[bufferwidth],ax
	mov	si,[fontnumber]
	shl	si,1
	mov	es,[grsegs+STARTFONT*2+si]
	mov	ax,[es:pcharheight]
	mov	[bufferheight],ax

	mov	si,OFFSET databuffer
	call	VWL_XORBuffer

	ret

ENDP

endif	;numfont

endif		; if fonts
