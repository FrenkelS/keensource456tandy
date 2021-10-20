/* Commander Keen 4 Tandy Version Source Code
 * Copyright (C) 2021 Frenkel Smeijers
 *
 * This file is primarily based on:
 * Reconstructed Commander Keen 4-6 Source Code
 * Copyright (C) 2021 K1n9_Duk3
 *
 * This file is primarily based on:
 * Catacomb 3-D Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// ID_CA.C

/*
=============================================================================

Id Software Caching Manager
---------------------------

Must be started BEFORE the memory manager, because it needs to get the headers
loaded into the data segment

=============================================================================
*/

#include "ID_HEADS.H"
#pragma hdrstop

#pragma warn -pro
#pragma warn -use

#define THREEBYTEGRSTARTS

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

typedef struct
{
	unsigned	RLEWtag;
	long		headeroffsets[100];
	byte		tileinfo[];
} mapfiletype;


/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

byte 		_seg	*tinf;
int			mapon;

unsigned	_seg	*mapsegs[3];
maptype		_seg	*mapheaderseg[NUMMAPS];
byte		_seg	*audiosegs[NUMSNDCHUNKS];
void		_seg	*grsegs[NUMCHUNKS];

byte		far	grneeded[NUMCHUNKS];
byte		ca_levelbit,ca_levelnum;

int			profilehandle,debughandle;

void	(*drawcachebox)		(char *title, unsigned numcache);
void	(*updatecachebox)	(void);
void	(*finishcachebox)	(void);

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

extern	long	far	CGAhead;
extern	long	far	TGAhead;
extern	long	far	EGAhead;
extern	byte	far	maphead;
extern	byte	far	audiohead;


long		_seg *grstarts;	// array of offsets in egagraph, -1 for sparse
long		_seg *audiostarts;	// array of offsets in audio / audiot


int			grhandle;		// handle to EGAGRAPH
int			maphandle;		// handle to MAPTEMP / GAMEMAPS
int			audiohandle;	// handle to AUDIOT / AUDIO

long		chunkcomplen,chunkexplen;

SDMode		oldsoundmode;



void	CAL_CarmackExpand (unsigned far *source, unsigned far *dest,
		unsigned length);


#ifdef THREEBYTEGRSTARTS
#define FILEPOSSIZE	3
//#define	GRFILEPOS(c) (*(long far *)(((byte far *)grstarts)+(c)*3)&0xffffff)
long GRFILEPOS(int c)
{
	long value;
	int	offset;

	offset = c*3;

	value = *(long far *)(((byte far *)grstarts)+offset);

	value &= 0x00ffffffl;

	if (value == 0xffffffl)
		value = -1;

	return value;
};
#else
#define FILEPOSSIZE	4
#define	GRFILEPOS(c) (grstarts[c])
#endif

/*
=============================================================================

					   LOW LEVEL ROUTINES

=============================================================================
*/

/*
============================
=
= CA_OpenDebug / CA_CloseDebug
=
= Opens a binary file with the handle "debughandle"
=
============================
*/

void CA_OpenDebug (void)
{
	unlink ("DEBUG.TXT");
	debughandle = open("DEBUG.TXT", O_CREAT | O_WRONLY | O_TEXT);
}

void CA_CloseDebug (void)
{
	close (debughandle);
}



/*
============================
=
= CAL_GetGrChunkLength
=
= Gets the length of an explicit length chunk (not tiles)
= The file pointer is positioned so the compressed data can be read in next.
=
============================
*/

void CAL_GetGrChunkLength (int chunk)
{
	lseek(grhandle,GRFILEPOS(chunk),SEEK_SET);
	read(grhandle,&chunkexplen,sizeof(chunkexplen));
	chunkcomplen = GRFILEPOS(chunk+1)-GRFILEPOS(chunk)-4;
}


/*
==========================
=
= CA_FarRead
=
= Read from a file to a far pointer
=
==========================
*/

boolean CA_FarRead (int handle, byte far *dest, long length)
{
	if (length>0xffffl)
		Quit ("CA_FarRead doesn't support 64K reads yet!");

asm		push	ds
asm		mov	bx,[handle]
asm		mov	cx,[WORD PTR length]
asm		mov	dx,[WORD PTR dest]
asm		mov	ds,[WORD PTR dest+2]
asm		mov	ah,0x3f				// READ w/handle
asm		int	21h
asm		pop	ds
asm		jnc	good
	errno = _AX;
	return	false;
good:
asm		cmp	ax,[WORD PTR length]
asm		je	done
	errno = EINVFMT;			// user manager knows this is bad read
	return	false;
done:
	return	true;
}


/*
==========================
=
= CA_SegWrite
=
= Write from a file to a far pointer
=
==========================
*/

boolean CA_FarWrite (int handle, byte far *source, long length)
{
	if (length>0xffffl)
		Quit ("CA_FarWrite doesn't support 64K reads yet!");

asm		push	ds
asm		mov	bx,[handle]
asm		mov	cx,[WORD PTR length]
asm		mov	dx,[WORD PTR source]
asm		mov	ds,[WORD PTR source+2]
asm		mov	ah,0x40			// WRITE w/handle
asm		int	21h
asm		pop	ds
asm		jnc	good
	errno = _AX;
	return	false;
good:
asm		cmp	ax,[WORD PTR length]
asm		je	done
	errno = ENOMEM;				// user manager knows this is bad write
	return	false;

done:
	return	true;
}

/*
============================================================================

		COMPRESSION routines, see JHUFF.C for more

============================================================================
*/



/*
======================
=
= CAL_Lzsa2Expand
=
=
======================
*/

void CAL_Lzsa2Expand (byte huge *source, byte huge *dest)
{
	unsigned sourceseg,sourceoff,destseg,destoff;

	source++;	// normalize
	source--;
	dest++;
	dest--;

	sourceseg = FP_SEG(source);
	sourceoff = FP_OFF(source);
	destseg = FP_SEG(dest);
	destoff = FP_OFF(dest);

asm	mov	si,[sourceoff]
asm	mov	di,[destoff]
asm	mov	es,[destseg]
asm	mov	ds,[sourceseg]
asm	push	bp

//  This code is primarily based on:
//
//  decompress_small.S - space-efficient decompressor implementation for 8088
//
//  Copyright (C) 2019 Emmanuel Marty
//
//  This software is provided 'as-is', without any express or implied
//  warranty.  In no event will the authors be held liable for any damages
//  arising from the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//  2. Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software.
//  3. This notice may not be removed or altered from any source distribution.

//  ---------------------------------------------------------------------------
//  Decompress raw LZSA2 block
//  inputs:
//  * ds:si: raw LZSA2 block
//  * es:di: output buffer
//  output:
//  * ax:    decompressed size
//  ---------------------------------------------------------------------------
//asm   push di                 // remember decompression offset
asm   cld                     // make string operations (lods, movs, stos..) move forward

asm   xor cx,cx
asm   mov bx,0100H
asm   xor bp,bp

decode_token:
asm   mov ax,cx               // clear ah - cx is zero from above or from after rep movsb in .copy_match
asm   lodsb                   // read token byte: XYZ|LL|MMMM
asm   mov dx,ax               // keep token in dl
   
asm   and al,018H             // isolate literals length in token (LL)
asm   mov cl,3
asm   shr al,cl               // shift literals length into place

asm   cmp al,03H              // LITERALS_RUN_LEN_V2?
asm   jne got_literals       // no, we have the full literals count from the token, go copy

//asm   call get_nibble        // get extra literals length nibble
asm   neg bh                  // nibble ready?
asm   jns has_nibble1
   
asm   xchg bx,ax
asm   lodsb                   // load two nibbles
asm   xchg bx,ax

has_nibble1:
asm   mov cl,4                // swap 4 high and low bits of nibble
asm   ror bl,cl
asm   mov cl,0FH
asm   and cl,bl

asm   add al,cl               // add len from token to nibble 
asm   cmp al,012H             // LITERALS_RUN_LEN_V2 + 15 ?
asm   jne got_literals       // if not, we have the full literals count, go copy

asm   lodsb                   // grab extra length byte
asm   add al,012H             // overflow?
asm   jnc got_literals       // if not, we have the full literals count, go copy

asm   lodsw                   // grab 16-bit extra length

got_literals:
asm   xchg cx,ax
asm   rep movsb               // copy cx literals from ds:si to es:di

asm   test dl,0C0h            // check match offset mode in token (X bit)
asm   js rep_match_or_large_offset

   // cmp dl,040H             // check if this is a 5 or 9-bit offset (Y bit)
                           // discovered via the test with bit 6 set
asm   xchg cx,ax              // clear ah - cx is zero from the rep movsb above
asm   jne offset_9_bit

                           // 5 bit offset
asm   cmp dl,020H             // test bit 5
//asm   call get_nibble_x
asm   cmc                     // carry set if bit 4 was set
asm   rcr al,1
//asm   call get_nibble        // get nibble for offset bits 0-3
asm   neg bh                  // nibble ready?
asm   jns has_nibble2
   
asm   xchg bx,ax
asm   lodsb                   // load two nibbles
asm   xchg bx,ax

has_nibble2:
asm   mov cl,4                // swap 4 high and low bits of nibble
asm   ror bl,cl
asm   mov cl,0FH
asm   and cl,bl

asm   or al,cl                // merge nibble
asm   rol al,1
asm   xor al,0E1H             // set offset bits 7-5 to 1
asm   jmp short dec_offset_top

offset_9_bit:             // 9 bit offset
asm   lodsb                   // get 8 bit offset from stream in A
asm   dec ah                  // set offset bits 15-8 to 1
asm   test dl,020H            // test bit Z (offset bit 8)
asm   je get_match_length
dec_offset_top:
asm   dec ah                  // clear bit 8 if Z bit is clear
                           // or set offset bits 15-8 to 1
asm   jmp short get_match_length

rep_match_or_large_offset:
   // cmp dl,0c0H             // check if this is a 13-bit offset or a 16-bit offset/rep match (Y bit)
asm   jpe rep_match_or_16_bit

                           // 13 bit offset

asm   cmp dl,0A0H             // test bit 5 (knowing that bit 7 is also set)
asm   xchg ah,al
//asm   call get_nibble_x
asm   cmc                     // carry set if bit 4 was set
asm   rcr al,1
//asm   call get_nibble        // get nibble for offset bits 0-3
asm   neg bh                  // nibble ready?
asm   jns has_nibble3
   
asm   xchg bx,ax
asm   lodsb                   // load two nibbles
asm   xchg bx,ax

has_nibble3:
asm   mov cl,4                // swap 4 high and low bits of nibble
asm   ror bl,cl
asm   mov cl,0FH
asm   and cl,bl

asm   or al,cl                // merge nibble
asm   rol al,1
asm   xor al,0E1H             // set offset bits 7-5 to 1

asm   sub al,2                // substract 512
asm   jmp short get_match_length_1

rep_match_or_16_bit:
asm   test dl,020H            // test bit Z (offset bit 8)
asm   jne repeat_match       // rep-match

                           // 16 bit offset
asm   lodsb                   // Get 2-byte match offset

get_match_length_1:
asm   xchg ah,al
asm   lodsb                   // load match offset bits 0-7

get_match_length:
asm   xchg bp,ax              // bp: offset
repeat_match:
asm   xchg ax,dx              // ax: original token
asm   and al,07H              // isolate match length in token (MMM)
asm   add al,2                // add MIN_MATCH_SIZE_V2

asm   cmp al,09H              // MIN_MATCH_SIZE_V2 + MATCH_RUN_LEN_V2?
asm   jne got_matchlen       // no, we have the full match length from the token, go copy

//asm   call get_nibble        // get extra literals length nibble
asm   neg bh                  // nibble ready?
asm   jns has_nibble4
   
asm   xchg bx,ax
asm   lodsb                   // load two nibbles
asm   xchg bx,ax

has_nibble4:
asm   mov cl,4                // swap 4 high and low bits of nibble
asm   ror bl,cl
asm   mov cl,0FH
asm   and cl,bl

asm   add al,cl               // add len from token to nibble 
asm   cmp al,018H             // MIN_MATCH_SIZE_V2 + MATCH_RUN_LEN_V2 + 15?
asm   jne got_matchlen       // no, we have the full match length from the token, go copy

asm   lodsb                   // grab extra length byte
asm   add al,018H             // overflow?
asm   jnc got_matchlen       // if not, we have the entire length
asm   je short done_decompressing // detect EOD code

asm   lodsw                   // grab 16-bit length

got_matchlen:
asm   xchg cx,ax              // copy match length into cx
asm   push ds                 // save ds:si (current pointer to compressed data)
asm   xchg si,ax          
asm   push es
asm   pop ds
asm   lea si,[bp+di]          // ds:si now points at back reference in output data
asm   rep movsb               // copy match
asm   xchg si,ax              // restore ds:si
asm   pop ds
asm   jmp decode_token       // go decode another token

done_decompressing:
//asm   pop ax                  // retrieve the original decompression offset
//asm   xchg di,ax              // compute decompressed size
//asm   sub ax,di

asm	pop	bp
asm	mov	ax,ss
asm	mov	ds,ax
asm	mov	es,ax
}



/*
======================
=
= CAL_CarmackExpand
=
= Length is the length of the EXPANDED data
=
======================
*/

#define NEARTAG	0xa7
#define FARTAG	0xa8

void CAL_CarmackExpand (unsigned far *source, unsigned far *dest, unsigned length)
{
	unsigned	ch,chhigh,count,offset;
	unsigned	far *copyptr, far *inptr, far *outptr;

	length/=2;

	inptr = source;
	outptr = dest;

	while (length)
	{
		ch = *inptr++;
		chhigh = ch>>8;
		if (chhigh == NEARTAG)
		{
			count = ch&0xff;
			if (!count)
			{				// have to insert a word containing the tag byte
				ch |= *((unsigned char far *)inptr)++;
				*outptr++ = ch;
				length--;
			}
			else
			{
				offset = *((unsigned char far *)inptr)++;
				copyptr = outptr - offset;
				length -= count;
				while (count--)
					*outptr++ = *copyptr++;
			}
		}
		else if (chhigh == FARTAG)
		{
			count = ch&0xff;
			if (!count)
			{				// have to insert a word containing the tag byte
				ch |= *((unsigned char far *)inptr)++;
				*outptr++ = ch;
				length --;
			}
			else
			{
				offset = *inptr++;
				copyptr = dest + offset;
				length -= count;
				while (count--)
					*outptr++ = *copyptr++;
			}
		}
		else
		{
			*outptr++ = ch;
			length --;
		}
	}
}



/*
======================
=
= CA_RLEWcompress
=
======================
*/

long CA_RLEWCompress (unsigned huge *source, long length, unsigned huge *dest,
  unsigned rlewtag)
{
  long complength;
  unsigned value,count,i;
  unsigned huge *start,huge *end;

  start = dest;

  end = source + (length+1)/2;

//
// compress it
//
  do
  {
	count = 1;
	value = *source++;
	while (*source == value && source<end)
	{
	  count++;
	  source++;
	}
	if (count>3 || value == rlewtag)
	{
    //
    // send a tag / count / value string
    //
      *dest++ = rlewtag;
      *dest++ = count;
      *dest++ = value;
    }
    else
    {
    //
    // send word without compressing
    //
      for (i=1;i<=count;i++)
	*dest++ = value;
	}

  } while (source<end);

  complength = 2*(dest-start);
  return complength;
}


/*
======================
=
= CA_RLEWexpand
= length is EXPANDED length
=
======================
*/

void CA_RLEWexpand (unsigned huge *source, unsigned huge *dest,long length,
  unsigned rlewtag)
{
//  unsigned value,count,i;
  unsigned huge *end;
  unsigned sourceseg,sourceoff,destseg,destoff,endseg,endoff;


//
// expand it
//
#if 0
  do
  {
	value = *source++;
	if (value != rlewtag)
	//
	// uncompressed
	//
	  *dest++=value;
	else
	{
	//
	// compressed string
	//
	  count = *source++;
	  value = *source++;
	  for (i=1;i<=count;i++)
	*dest++ = value;
	}
  } while (dest<end);
#endif

  end = dest + (length)/2;
  sourceseg = FP_SEG(source);
  sourceoff = FP_OFF(source);
  destseg = FP_SEG(dest);
  destoff = FP_OFF(dest);
  endseg = FP_SEG(end);
  endoff = FP_OFF(end);


//
// ax = source value
// bx = tag value
// cx = repeat counts
// dx = scratch
//
// NOTE: A repeat count that produces 0xfff0 bytes can blow this!
//

asm	mov	bx,rlewtag
asm	mov	si,sourceoff
asm	mov	di,destoff
asm	mov	es,destseg
asm	mov	ds,sourceseg

expand:
asm	lodsw
asm	cmp	ax,bx
asm	je	repeat
asm	stosw
asm	jmp	next

repeat:
asm	lodsw
asm	mov	cx,ax		// repeat count
asm	lodsw			// repeat value
asm	rep stosw

next:

asm	cmp	si,0x10		// normalize ds:si
asm  	jb	sinorm
asm	mov	ax,si
asm	shr	ax,1
asm	shr	ax,1
asm	shr	ax,1
asm	shr	ax,1
asm	mov	dx,ds
asm	add	dx,ax
asm	mov	ds,dx
asm	and	si,0xf
sinorm:
asm	cmp	di,0x10		// normalize es:di
asm  	jb	dinorm
asm	mov	ax,di
asm	shr	ax,1
asm	shr	ax,1
asm	shr	ax,1
asm	shr	ax,1
asm	mov	dx,es
asm	add	dx,ax
asm	mov	es,dx
asm	and	di,0xf
dinorm:

asm	cmp     di,ss:endoff
asm	jne	expand
asm	mov	ax,es
asm	cmp	ax,ss:endseg
asm	jb	expand

asm	mov	ax,ss
asm	mov	ds,ax

}



/*
=============================================================================

					 CACHE MANAGER ROUTINES

=============================================================================
*/


/*
======================
=
= CAL_SetupGrFile
=
======================
*/

void CAL_SetupGrFile (void)
{
	int handle;
	memptr compseg;

#ifdef GRHEADERLINKED

#if GRMODE == EGAGR
	grstarts = (long _seg *)FP_SEG(&EGAhead);
#elif GRMODE == CGAGR
	grstarts = (long _seg *)FP_SEG(&CGAhead);
#elif GRMODE == TGAGR
	grstarts = (long _seg *)FP_SEG(&TGAhead);
#endif

#else

//
// load the data offsets from ???head.ext
//
	MM_GetPtr (&(memptr)grstarts,(NUMCHUNKS+1)*FILEPOSSIZE);

	if ((handle = open(GREXT"HEAD."EXTENSION,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		Quit ("Can't open "GREXT"HEAD."EXTENSION"!");

	CA_FarRead(handle, (memptr)grstarts, (NUMCHUNKS+1)*FILEPOSSIZE);

	close(handle);


#endif

//
// Open the graphics file, leaving it open until the game is finished
//
	grhandle = open(GREXT"GRAPH."EXTENSION, O_RDONLY | O_BINARY);
	if (grhandle == -1)
		Quit ("Cannot open "GREXT"GRAPH."EXTENSION"!");


//
// load the pic and sprite headers into the arrays in the data segment
//
#if NUMPICS>0
	MM_GetPtr(&(memptr)pictable,NUMPICS*sizeof(pictabletype));
	CAL_GetGrChunkLength(STRUCTPIC);		// position file pointer
	MM_GetPtr(&compseg,chunkcomplen);
	CA_FarRead (grhandle,compseg,chunkcomplen);
	CAL_Lzsa2Expand (compseg, (byte huge *)pictable);
	MM_FreePtr(&compseg);
#endif

#if NUMPICM>0
	MM_GetPtr(&(memptr)picmtable,NUMPICM*sizeof(pictabletype));
	CAL_GetGrChunkLength(STRUCTPICM);		// position file pointer
	MM_GetPtr(&compseg,chunkcomplen);
	CA_FarRead (grhandle,compseg,chunkcomplen);
	CAL_Lzsa2Expand (compseg, (byte huge *)picmtable);
	MM_FreePtr(&compseg);
#endif

#if NUMSPRITES>0
	MM_GetPtr(&(memptr)spritetable,NUMSPRITES*sizeof(spritetabletype));
	CAL_GetGrChunkLength(STRUCTSPRITE);	// position file pointer
	MM_GetPtr(&compseg,chunkcomplen);
	CA_FarRead (grhandle,compseg,chunkcomplen);
	CAL_Lzsa2Expand (compseg, (byte huge *)spritetable);
	MM_FreePtr(&compseg);
#endif

}

//==========================================================================


/*
======================
=
= CAL_SetupMapFile
=
======================
*/

void CAL_SetupMapFile (void)
{
	int handle;
	long length;

//
// load maphead.ext (offsets and tileinfo for map file)
//
#ifndef MAPHEADERLINKED
	if ((handle = open("MAPHEAD."EXTENSION,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		Quit ("Can't open MAPHEAD."EXTENSION"!");
	length = filelength(handle);
	MM_GetPtr (&(memptr)tinf,length);
	CA_FarRead(handle, tinf, length);
	close(handle);
#else

	tinf = (byte _seg *)FP_SEG(&maphead);

#endif

//
// open the data file
//
#ifdef MAPHEADERLINKED
	if ((maphandle = open("GAMEMAPS."EXTENSION,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		Quit ("Can't open GAMEMAPS."EXTENSION"!");
#else
	if ((maphandle = open("MAPTEMP."EXTENSION,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		Quit ("Can't open MAPTEMP."EXTENSION"!");
#endif
}

//==========================================================================


/*
======================
=
= CAL_SetupAudioFile
=
======================
*/

void CAL_SetupAudioFile (void)
{
	int handle;
	long length;

//
// load maphead.ext (offsets and tileinfo for map file)
//
#ifndef AUDIOHEADERLINKED
	if ((handle = open("AUDIOHED."EXTENSION,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		Quit ("Can't open AUDIOHED."EXTENSION"!");
	length = filelength(handle);
	MM_GetPtr (&(memptr)audiostarts,length);
	CA_FarRead(handle, (byte far *)audiostarts, length);
	close(handle);
#else
	audiostarts = (long _seg *)FP_SEG(&audiohead);
#endif

//
// open the data file
//
#ifndef AUDIOHEADERLINKED
	if ((audiohandle = open("AUDIOT."EXTENSION,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		Quit ("Can't open AUDIOT."EXTENSION"!");
#else
	if ((audiohandle = open("AUDIO."EXTENSION,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		Quit ("Can't open AUDIO."EXTENSION"!");
#endif
}

//==========================================================================


/*
======================
=
= CA_Startup
=
= Open all files and load in headers
=
======================
*/

void CA_Startup (void)
{
#ifdef PROFILE
	unlink ("PROFILE.TXT");
	profilehandle = open("PROFILE.TXT", O_CREAT | O_WRONLY | O_TEXT);
#endif

#ifndef NOMAPS
	CAL_SetupMapFile ();
#endif
#ifndef NOGRAPHICS
	CAL_SetupGrFile ();
#endif
#ifndef NOAUDIO
	CAL_SetupAudioFile ();
#endif

	mapon = -1;
	ca_levelbit = 1;
	ca_levelnum = 0;
}

//==========================================================================


/*
======================
=
= CA_Shutdown
=
= Closes all files
=
======================
*/

void CA_Shutdown (void)
{
#ifdef PROFILE
	close (profilehandle);
#endif

	close (maphandle);
	close (grhandle);
	close (audiohandle);
}

//===========================================================================

/*
======================
=
= CA_CacheAudioChunk
=
======================
*/

void CA_CacheAudioChunk (int chunk)
{
	long	pos,compressed;
#ifdef AUDIOHEADERLINKED
	long	expanded;
	memptr	bigbufferseg;
	byte	far *source;
#endif

	if (audiosegs[chunk])
	{
		MM_SetPurge (&(memptr)audiosegs[chunk],0);
		return;							// allready in memory
	}

//
// load the chunk into a buffer, either the miscbuffer if it fits, or allocate
// a larger buffer
//
	pos = audiostarts[chunk];
	compressed = audiostarts[chunk+1]-pos;

	lseek(audiohandle,pos,SEEK_SET);

#ifndef AUDIOHEADERLINKED

	MM_GetPtr (&(memptr)audiosegs[chunk],compressed);
	if (mmerror)
		return;

	CA_FarRead(audiohandle,audiosegs[chunk],compressed);

#else

	if (compressed<=BUFFERSIZE)
	{
		CA_FarRead(audiohandle,bufferseg,compressed);
		source = bufferseg;
	}
	else
	{
		MM_GetPtr(&bigbufferseg,compressed);
		if (mmerror)
			return;
		MM_SetLock (&bigbufferseg,true);
		CA_FarRead(audiohandle,bigbufferseg,compressed);
		source = bigbufferseg;
	}

	expanded = *(long far *)source;
	source += 4;			// skip over length
	MM_GetPtr (&(memptr)audiosegs[chunk],expanded);
	if (mmerror)
		goto done;
	CAL_Lzsa2Expand (source,audiosegs[chunk]);

done:
	if (compressed>BUFFERSIZE)
		MM_FreePtr(&bigbufferseg);
#endif
}

//===========================================================================

/*
======================
=
= CA_LoadAllSounds
=
= Purges all sounds, then loads all new ones (mode switch)
=
======================
*/

void CA_LoadAllSounds (void)
{
	unsigned	start,i;

	switch (oldsoundmode)
	{
	case sdm_Off:
		goto cachein;
	case sdm_PC:
		start = STARTPCSOUNDS;
		break;
	case sdm_AdLib:
		start = STARTADLIBSOUNDS;
		break;
	}

	for (i=0;i<NUMSOUNDS;i++,start++)
		if (audiosegs[start])
			MM_SetPurge (&(memptr)audiosegs[start],3);		// make purgable

cachein:

	switch (SoundMode)
	{
	case sdm_Off:
		return;
	case sdm_PC:
		start = STARTPCSOUNDS;
		break;
	case sdm_AdLib:
		start = STARTADLIBSOUNDS;
		break;
	}

	for (i=0;i<NUMSOUNDS;i++,start++)
		CA_CacheAudioChunk (start);

	oldsoundmode = SoundMode;
}

//===========================================================================

#if GRMODE == EGAGR

/*
======================
=
= CAL_ShiftSprite
=
= Make a shifted (one byte wider) copy of a sprite into another area
=
======================
*/

unsigned	static	sheight,swidth;

void CAL_ShiftSprite (unsigned segment,unsigned source,unsigned dest,
	unsigned width, unsigned height, unsigned pixshift)
{

	sheight = height;		// because we are going to reassign bp
	swidth = width;

asm	mov	ax,[segment]
asm	mov	ds,ax		// source and dest are in same segment, and all local

asm	mov	bx,[source]
asm	mov	di,[dest]

asm	mov	bp,[pixshift]
asm	shl	bp,1
asm	mov	bp,WORD PTR [shifttabletable+bp]	// bp holds pointer to shift table

//
// table shift the mask
//
asm	mov	dx,[ss:sheight]

domaskrow:

asm	mov	BYTE PTR [di],255	// 0xff first byte
asm	mov	cx,ss:[swidth]

domaskbyte:

asm	mov	al,[bx]				// source
asm	not	al
asm	inc	bx					// next source byte
asm	xor	ah,ah
asm	shl	ax,1
asm	mov	si,ax
asm	mov	ax,[bp+si]			// table shift into two bytes
asm	not	ax
asm	and	[di],al				// and with first byte
asm	inc	di
asm	mov	[di],ah				// replace next byte

asm	loop	domaskbyte

asm	inc	di					// the last shifted byte has 1s in it
asm	dec	dx
asm	jnz	domaskrow

//
// table shift the data
//
asm	mov	dx,ss:[sheight]
asm	shl	dx,1
asm	shl	dx,1				// four planes of data

dodatarow:

asm	mov	BYTE PTR [di],0		// 0 first byte
asm	mov	cx,ss:[swidth]

dodatabyte:

asm	mov	al,[bx]				// source
asm	inc	bx					// next source byte
asm	xor	ah,ah
asm	shl	ax,1
asm	mov	si,ax
asm	mov	ax,[bp+si]			// table shift into two bytes
asm	or	[di],al				// or with first byte
asm	inc	di
asm	mov	[di],ah				// replace next byte

asm	loop	dodatabyte

asm	inc	di					// the last shifted byte has 0s in it
asm	dec	dx
asm	jnz	dodatarow

//
// done
//

asm	mov	ax,ss				// restore data segment
asm	mov	ds,ax

}

#endif

//===========================================================================

/*
======================
=
= CAL_CacheSprite
=
= Generate shifts and set up sprite structure for a given sprite
=
======================
*/

void CAL_CacheSprite (int chunk, byte far *compressed)
{
	int i;
	unsigned shiftstarts[5];
	unsigned smallplane,bigplane,expanded;
	spritetabletype far *spr;
	spritetype _seg *dest;

#if GRMODE == CGAGR || GRMODE == TGAGR
//
// CGA and TGA have no pel panning, so shifts are never needed
//
	spr = &spritetable[chunk-STARTSPRITES];
	smallplane = spr->width*spr->height;
	MM_GetPtr (&grsegs[chunk],smallplane*2+MAXSHIFTS*6);
	if (mmerror)
		return;
	dest = (spritetype _seg *)grsegs[chunk];
	dest->sourceoffset[0] = MAXSHIFTS*6;	// start data after 3 unsigned tables
	dest->planesize[0] = smallplane;
	dest->width[0] = spr->width;

//
// expand the unshifted shape
//
	CAL_Lzsa2Expand (compressed, &dest->data[0]);

#endif


#if GRMODE == EGAGR

//
// calculate sizes
//
	spr = &spritetable[chunk-STARTSPRITES];
	smallplane = spr->width*spr->height;
	bigplane = (spr->width+1)*spr->height;

	shiftstarts[0] = MAXSHIFTS*6;	// start data after 3 unsigned tables
	shiftstarts[1] = shiftstarts[0] + smallplane*5;	// 5 planes in a sprite
	shiftstarts[2] = shiftstarts[1] + bigplane*5;
	shiftstarts[3] = shiftstarts[2] + bigplane*5;
	shiftstarts[4] = shiftstarts[3] + bigplane*5;	// nothing ever put here

	expanded = shiftstarts[spr->shifts];
	MM_GetPtr (&grsegs[chunk],expanded);
	if (mmerror)
		return;
	dest = (spritetype _seg *)grsegs[chunk];

//
// expand the unshifted shape
//
	CAL_Lzsa2Expand (compressed, &dest->data[0]);

//
// make the shifts!
//
	switch (spr->shifts)
	{
	case	1:
		for (i=0;i<4;i++)
		{
			dest->sourceoffset[i] = shiftstarts[0];
			dest->planesize[i] = smallplane;
			dest->width[i] = spr->width;
		}
		break;

	case	2:
		for (i=0;i<2;i++)
		{
			dest->sourceoffset[i] = shiftstarts[0];
			dest->planesize[i] = smallplane;
			dest->width[i] = spr->width;
		}
		for (i=2;i<4;i++)
		{
			dest->sourceoffset[i] = shiftstarts[1];
			dest->planesize[i] = bigplane;
			dest->width[i] = spr->width+1;
		}
		CAL_ShiftSprite ((unsigned)grsegs[chunk],dest->sourceoffset[0],
			dest->sourceoffset[2],spr->width,spr->height,4);
		break;

	case	4:
		dest->sourceoffset[0] = shiftstarts[0];
		dest->planesize[0] = smallplane;
		dest->width[0] = spr->width;

		dest->sourceoffset[1] = shiftstarts[1];
		dest->planesize[1] = bigplane;
		dest->width[1] = spr->width+1;
		CAL_ShiftSprite ((unsigned)grsegs[chunk],dest->sourceoffset[0],
			dest->sourceoffset[1],spr->width,spr->height,2);

		dest->sourceoffset[2] = shiftstarts[2];
		dest->planesize[2] = bigplane;
		dest->width[2] = spr->width+1;
		CAL_ShiftSprite ((unsigned)grsegs[chunk],dest->sourceoffset[0],
			dest->sourceoffset[2],spr->width,spr->height,4);

		dest->sourceoffset[3] = shiftstarts[3];
		dest->planesize[3] = bigplane;
		dest->width[3] = spr->width+1;
		CAL_ShiftSprite ((unsigned)grsegs[chunk],dest->sourceoffset[0],
			dest->sourceoffset[3],spr->width,spr->height,6);

		break;

	default:
		Quit ("CAL_CacheSprite: Bad shifts number!");
	}

#endif
}

//===========================================================================


/*
======================
=
= CAL_ExpandGrChunk
=
= Does whatever is needed with a pointer to a compressed chunk
=
======================
*/

void CAL_ExpandGrChunk (int chunk, byte far *source)
{
	long	expanded;


	if (chunk >= STARTTILE8 && chunk < STARTEXTERNS)
	{
	//
	// expanded sizes of tile8/16/32 are implicit
	//

#if GRMODE == EGAGR
#define BLOCK		32
#define MASKBLOCK	40
#endif

#if GRMODE == CGAGR
#define BLOCK		16
#define MASKBLOCK	32
#endif

#if GRMODE == TGAGR
#define BLOCK		32
#define MASKBLOCK	64
#endif

		if (chunk<STARTTILE8M)			// tile 8s are all in one chunk!
			expanded = BLOCK*NUMTILE8;
		else if (chunk<STARTTILE16)
			expanded = MASKBLOCK*NUMTILE8M;
		else if (chunk<STARTTILE16M)	// all other tiles are one/chunk
			expanded = BLOCK*4;
		else if (chunk<STARTTILE32)
			expanded = MASKBLOCK*4;
		else if (chunk<STARTTILE32M)
			expanded = BLOCK*16;
		else
			expanded = MASKBLOCK*16;
	}
	else
	{
	//
	// everything else has an explicit size longword
	//
		expanded = *(long far *)source;
		source += 4;			// skip over length
	}

//
// allocate final space, decompress it, and free bigbuffer
// Sprites need to have shifts made and various other junk
//
	if (chunk>=STARTSPRITES && chunk< STARTTILE8)
		CAL_CacheSprite(chunk,source);
	else
	{
		MM_GetPtr (&grsegs[chunk],expanded);
		if (mmerror)
			return;
		CAL_Lzsa2Expand (source,grsegs[chunk]);
	}
}


/*
======================
=
= CA_CacheGrChunk
=
= Makes sure a given chunk is in memory, loadiing it if needed
=
======================
*/

void CA_CacheGrChunk (int chunk)
{
	long	pos,compressed;
	memptr	bigbufferseg;
	byte	far *source;
	int		next;

	grneeded[chunk] |= ca_levelbit;		// make sure it doesn't get removed
	if (grsegs[chunk])
	{
		MM_SetPurge (&grsegs[chunk],0);
		return;							// allready in memory
	}

//
// load the chunk into a buffer, either the miscbuffer if it fits, or allocate
// a larger buffer
//
	pos = GRFILEPOS(chunk);
	if (pos<0)							// $FFFFFFFF start is a sparse tile
	  return;

	next = chunk +1;
	while (GRFILEPOS(next) == -1)		// skip past any sparse tiles
		next++;

	compressed = GRFILEPOS(next)-pos;

	lseek(grhandle,pos,SEEK_SET);

	if (compressed<=BUFFERSIZE)
	{
		CA_FarRead(grhandle,bufferseg,compressed);
		source = bufferseg;
	}
	else
	{
		MM_GetPtr(&bigbufferseg,compressed);
		MM_SetLock (&bigbufferseg,true);
		CA_FarRead(grhandle,bigbufferseg,compressed);
		source = bigbufferseg;
	}

	CAL_ExpandGrChunk (chunk,source);

	if (compressed>BUFFERSIZE)
		MM_FreePtr(&bigbufferseg);
}



//==========================================================================

/*
======================
=
= CA_CacheMap
=
======================
*/

void CA_CacheMap (int mapnum)
{
	long	pos,compressed;
	int		plane;
	memptr	*dest,bigbufferseg;
	unsigned	size;
	unsigned	far	*source;
#ifdef MAPHEADERLINKED
	memptr	buffer2seg;
	long	expanded;
#endif


//
// free up memory from last map
//
	if (mapon>-1 && mapheaderseg[mapon])
		MM_SetPurge (&(memptr)mapheaderseg[mapon],3);
	for (plane=0;plane<MAPPLANES;plane++)
		if (mapsegs[plane])
			MM_FreePtr (&(memptr)mapsegs[plane]);

	mapon = mapnum;


//
// load map header
// The header will be cached if it is still around
//
	if (!mapheaderseg[mapnum])
	{
		pos = ((mapfiletype	_seg *)tinf)->headeroffsets[mapnum];
		if (pos<0)						// $FFFFFFFF start is a sparse map
		  Quit ("CA_CacheMap: Tried to load a non existent map!");

		MM_GetPtr(&(memptr)mapheaderseg[mapnum],sizeof(maptype));
		lseek(maphandle,pos,SEEK_SET);
		CA_FarRead (maphandle,(memptr)mapheaderseg[mapnum],sizeof(maptype));
	}
	else
		MM_SetPurge (&(memptr)mapheaderseg[mapnum],0);

//
// load the planes in
// If a plane's pointer still exists it will be overwritten (levels are
// allways reloaded, never cached)
//

	size = mapheaderseg[mapnum]->width * mapheaderseg[mapnum]->height * 2;

	for (plane = 0; plane<MAPPLANES; plane++)
	{
		pos = mapheaderseg[mapnum]->planestart[plane];
		compressed = mapheaderseg[mapnum]->planelength[plane];

		if (!compressed)
			continue;		// the plane is not used in this game

		dest = &(memptr)mapsegs[plane];
		MM_GetPtr(dest,size);

		lseek(maphandle,pos,SEEK_SET);
		if (compressed<=BUFFERSIZE)
			source = bufferseg;
		else
		{
			MM_GetPtr(&bigbufferseg,compressed);
			MM_SetLock (&bigbufferseg,true);
			source = bigbufferseg;
		}

		CA_FarRead(maphandle,(byte far *)source,compressed);
#ifdef MAPHEADERLINKED
		//
		// unhuffman, then unRLEW
		// The huffman'd chunk has a two byte expanded length first
		// The resulting RLEW chunk also does, even though it's not really
		// needed
		//
		expanded = *source;
		source++;
		MM_GetPtr (&buffer2seg,expanded);
		CAL_CarmackExpand (source, (unsigned far *)buffer2seg,expanded);
		CA_RLEWexpand (((unsigned far *)buffer2seg)+1,*dest,size,
		((mapfiletype _seg *)tinf)->RLEWtag);
		MM_FreePtr (&buffer2seg);

#else
		//
		// unRLEW, skipping expanded length
		//
		CA_RLEWexpand (source+1, *dest,size,
		((mapfiletype _seg *)tinf)->RLEWtag);
#endif

		if (compressed>BUFFERSIZE)
			MM_FreePtr(&bigbufferseg);
	}
}

//===========================================================================

/*
======================
=
= CA_UpLevel
=
= Goes up a bit level in the needed lists and clears it out.
= Everything is made purgable
=
======================
*/

void CA_UpLevel (void)
{
	if (ca_levelnum==7)
		Quit ("CA_UpLevel: Up past level 7!");

	ca_levelbit<<=1;
	ca_levelnum++;
}

//===========================================================================

/*
======================
=
= CA_DownLevel
=
= Goes down a bit level in the needed lists and recaches
= everything from the lower level
=
======================
*/

void CA_DownLevel (void)
{
	if (!ca_levelnum)
		Quit ("CA_DownLevel: Down past level 0!");
	ca_levelbit>>=1;
	ca_levelnum--;
	CA_CacheMarks(NULL);
}

//===========================================================================

/*
======================
=
= CA_ClearMarks
=
= Clears out all the marks at the current level
=
======================
*/

void CA_ClearMarks (void)
{
	int i;

	for (i=0;i<NUMCHUNKS;i++)
		grneeded[i]&=~ca_levelbit;
}


//===========================================================================

/*
======================
=
= CA_FreeGraphics
=
======================
*/

void CA_FreeGraphics (void)
{
	int	i;

	for (i=0;i<NUMCHUNKS;i++)
		if (grsegs[i])
			MM_SetPurge (&(memptr)grsegs[i],3);
}


/*
======================
=
= CA_SetAllPurge
=
= Make everything possible purgable
=
======================
*/

void CA_SetAllPurge (void)
{
	int i;

	CA_ClearMarks ();

//
// free cursor sprite and background save
//
	VW_FreeCursor ();

//
// free map headers and map planes
//
	for (i=0;i<NUMMAPS;i++)
		if (mapheaderseg[i])
			MM_SetPurge (&(memptr)mapheaderseg[i],3);

	for (i=0;i<3;i++)
		if (mapsegs[i])
			MM_FreePtr (&(memptr)mapsegs[i]);

//
// free sounds
//
	for (i=0;i<NUMSNDCHUNKS;i++)
		if (audiosegs[i])
			MM_SetPurge (&(memptr)audiosegs[i],3);

//
// free graphics
//
	CA_FreeGraphics ();
}


void CA_SetGrPurge (void)
{
	int i;

//
// free graphics
//
	for (i=0;i<NUMCHUNKS;i++)
		if (grsegs[i])
			MM_SetPurge (&(memptr)grsegs[i],3);
}


//===========================================================================


/*
======================
=
= CA_CacheMarks
=
======================
*/
#define MAXEMPTYREAD	1024

void CA_CacheMarks (char *title)
{
	boolean dialog;
	int 	i,next,numcache;
	long	pos,endpos,nextpos,nextendpos,compressed;
	long	bufferstart,bufferend;	// file position of general buffer
	byte	far *source;
	memptr	bigbufferseg;

	dialog = (title!=NULL);

	numcache = 0;
//
// go through and make everything not needed purgable
//
	for (i=0;i<NUMCHUNKS;i++)
		if (grneeded[i]&ca_levelbit)
		{
			if (grsegs[i])					// its allready in memory, make
				MM_SetPurge(&grsegs[i],0);	// sure it stays there!
			else
				numcache++;
		}
		else
		{
			if (grsegs[i])					// not needed, so make it purgeable
				MM_SetPurge(&grsegs[i],3);
		}

	if (!numcache)			// nothing to cache!
		return;

	if (dialog)
	{
#ifdef PROFILE
		write(profilehandle,title,strlen(title));
		write(profilehandle,"\n",1);
#endif
		if (drawcachebox)
			drawcachebox(title,numcache);
	}

//
// go through and load in anything still needed
//
	bufferstart = bufferend = 0;		// nothing good in buffer now

	for (i=0;i<NUMCHUNKS;i++)
		if ( (grneeded[i]&ca_levelbit) && !grsegs[i])
		{
//
// update thermometer
//
			if (dialog && updatecachebox)
				updatecachebox ();

			pos = GRFILEPOS(i);
			if (pos<0)
				continue;

			next = i +1;
			while (GRFILEPOS(next) == -1)		// skip past any sparse tiles
				next++;

			compressed = GRFILEPOS(next)-pos;
			endpos = pos+compressed;

			if (compressed<=BUFFERSIZE)
			{
				if (bufferstart<=pos
				&& bufferend>= endpos)
				{
				// data is allready in buffer
					source = (byte _seg *)bufferseg+(pos-bufferstart);
				}
				else
				{
				// load buffer with a new block from disk
				// try to get as many of the needed blocks in as possible
					while ( next < NUMCHUNKS )
					{
						while (next < NUMCHUNKS &&
						!(grneeded[next]&ca_levelbit && !grsegs[next]))
							next++;
						if (next == NUMCHUNKS)
							continue;

						nextpos = GRFILEPOS(next);
						while (GRFILEPOS(++next) == -1)	// skip past any sparse tiles
							;
						nextendpos = GRFILEPOS(next);
						if (nextpos - endpos <= MAXEMPTYREAD
						&& nextendpos-pos <= BUFFERSIZE)
							endpos = nextendpos;
						else
							next = NUMCHUNKS;			// read pos to posend
					}

					lseek(grhandle,pos,SEEK_SET);
					CA_FarRead(grhandle,bufferseg,endpos-pos);
					bufferstart = pos;
					bufferend = endpos;
					source = bufferseg;
				}
			}
			else
			{
			// big chunk, allocate temporary buffer
				MM_GetPtr(&bigbufferseg,compressed);
				if (mmerror)
					return;
				MM_SetLock (&bigbufferseg,true);
				lseek(grhandle,pos,SEEK_SET);
				CA_FarRead(grhandle,bigbufferseg,compressed);
				source = bigbufferseg;
			}

			CAL_ExpandGrChunk (i,source);
			if (mmerror)
				return;

			if (compressed>BUFFERSIZE)
				MM_FreePtr(&bigbufferseg);

		}

//
// finish up any thermometer remnants
//
		if (dialog && finishcachebox)
			finishcachebox();
}

