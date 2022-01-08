/* Commander Keen 4 Tandy Version Source Code
 * Copyright (C) 2021-2022 Frenkel Smeijers
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

// NEWMM.C

/*
=============================================================================

		   ID software memory manager
		   --------------------------

Primary coder: John Carmack

RELIES ON
---------
Quit (char *error) function


WORK TO DO
----------
MM_SizePtr to change the size of a given pointer

EMS / XMS unmanaged routines

=============================================================================
*/

#include "ID_HEADS.H"
#pragma hdrstop

#pragma warn -pro
#pragma warn -use

/*
=============================================================================

							LOCAL INFO

=============================================================================
*/

#define SAVENEARHEAP	0x400		// space to leave in data segment
#define SAVEFARHEAP		0			// space to leave in far heap

#define MAXBLOCKS		918

#define LOCKBIT		0x80	// if set in attributes, block cannot be moved
#define PURGEBIT	1		// 0= unpurgeable, 1= purge
#define BASEATTRIBUTES	0	// unlocked, non purgeable

#define ISLOCKED(x)		(mmblocks[x].attributes&LOCKBIT)
#define ISPURGEABLE(x)	(mmblocks[x].attributes&PURGEBIT)

#define MAXUMBS		10

typedef struct mmblockstruct
{
	unsigned	start,length;
	unsigned	attributes;
	memptr		*useptr;	// pointer to the segment start
	int			next;
} mmblocktype;


#define FREEBLOCK(x) {*mmblocks[x].useptr=NULL;mmblocks[x].next=mmfree;mmfree=x;}


//--------

#define	EMS_INT			0x67

#define	EMS_STATUS		0x40
#define	EMS_GETFRAME	0x41
#define	EMS_GETPAGES	0x42
#define	EMS_ALLOCPAGES	0x43
#define	EMS_MAPPAGE		0x44
#define	EMS_FREEPAGES	0x45
#define	EMS_VERSION		0x46

//--------

#define	XMS_VERSION		0x00

#define	XMS_ALLOCHMA	0x01
#define	XMS_FREEHMA		0x02

#define	XMS_GENABLEA20	0x03
#define	XMS_GDISABLEA20	0x04
#define	XMS_LENABLEA20	0x05
#define	XMS_LDISABLEA20	0x06
#define	XMS_QUERYA20	0x07

#define	XMS_QUERYREE	0x08
#define	XMS_ALLOC		0x09
#define	XMS_FREE		0x0A
#define	XMS_MOVE		0x0B
#define	XMS_LOCK		0x0C
#define	XMS_UNLOCK		0x0D
#define	XMS_GETINFO		0x0E
#define	XMS_RESIZE		0x0F

#define	XMS_ALLOCUMB	0x10
#define	XMS_FREEUMB		0x11

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

mminfotype	mminfo;
memptr		bufferseg;
boolean		mmerror;

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

boolean		mmstarted;

void far	*farheap;
void		*nearheap;

mmblocktype	far mmblocks[MAXBLOCKS];
int 		mmfree, mmrover;

boolean		bombonerror;

unsigned	totalEMSpages,freeEMSpages,EMSpageframe,EMSpagesmapped,EMShandle;

void		(* XMSaddr) (void);		// far pointer to XMS driver

unsigned	numUMBs,UMBbase[MAXUMBS];

//==========================================================================

//
// local prototypes
//

boolean		MML_CheckForEMS (void);
void 		MML_ShutdownEMS (void);
void 		MML_MapEMS (void);
boolean 	MML_CheckForXMS (void);
void 		MML_ShutdownXMS (void);
void		MML_SortMem (void);

//==========================================================================

/*
======================
=
= MML_CheckForEMS
=
= Routine from p36 of Extending DOS
=
=======================
*/

char	emmname[9] = "EMMXXXX0";

boolean MML_CheckForEMS (void)
{
asm	mov	dx,OFFSET emmname[0]
asm	mov	ax,0x3d00
asm	int	0x21		// try to open EMMXXXX0 device
asm	jc	error

asm	mov	bx,ax
asm	mov	ax,0x4400

asm	int	0x21		// get device info
asm	jc	error

asm	and	dx,0x80
asm	jz	error

asm	mov	ax,0x4407

asm	int	0x21		// get status
asm	jc	error
asm	or	al,al
asm	jz	error

asm	mov	ah,0x3e
asm	int	0x21		// close handle
asm	jc	error

//
// EMS is good
//
  return true;

error:
//
// EMS is bad
//
  return false;
}


/*
======================
=
= MML_SetupEMS
=
=======================
*/

void MML_SetupEMS (void)
{
	char	str[80],str2[10];
	unsigned	error;

	totalEMSpages = freeEMSpages = EMSpageframe = EMSpagesmapped = 0;

asm {
	mov	ah,EMS_STATUS
	int	EMS_INT						// make sure EMS hardware is present
	or	ah,ah
	jnz	error

	mov	ah,EMS_VERSION
	int	EMS_INT
	or	ah,ah
	jnz	error
	cmp	al,0x32						// only work on ems 3.2 or greater
	jb	error

	mov	ah,EMS_GETFRAME
	int	EMS_INT						// find the page frame address
	or	ah,ah
	jnz	error
	mov	[EMSpageframe],bx

	mov	ah,EMS_GETPAGES
	int	EMS_INT						// find out how much EMS is there
	or	ah,ah
	jnz	error
	mov	[totalEMSpages],dx
	mov	[freeEMSpages],bx
	or	bx,bx
	jz	noEMS						// no EMS at all to allocate

	cmp	bx,4
	jle	getpages					// there is only 1,2,3,or 4 pages
	mov	bx,4						// we can't use more than 4 pages
	}

getpages:
asm {
	mov	[EMSpagesmapped],bx
	mov	ah,EMS_ALLOCPAGES			// allocate up to 64k of EMS
	int	EMS_INT
	or	ah,ah
	jnz	error
	mov	[EMShandle],dx
	}
	return;

error:
	error = _AH;
	strcpy (str,"MML_SetupEMS: EMS error 0x");
	itoa(error,str2,16);
	strcpy (str,str2);
	Quit (str);

noEMS:
;
}


/*
======================
=
= MML_ShutdownEMS
=
=======================
*/

void MML_ShutdownEMS (void)
{
	if (!EMShandle)
		return;

asm	{
	mov	ah,EMS_FREEPAGES
	mov	dx,[EMShandle]
	int	EMS_INT
	or	ah,ah
	jz	ok
	}

	Quit ("MML_ShutdownEMS: Error freeing EMS!");

ok:
;
}

/*
====================
=
= MML_MapEMS
=
= Maps the 64k of EMS used by memory manager into the page frame
= for general use.  This only needs to be called if you are keeping
= other things in EMS.
=
====================
*/

void MML_MapEMS (void)
{
	char	str[80],str2[10];
	unsigned	error;
	int	i;

	for (i=0;i<EMSpagesmapped;i++)
	{
	asm	{
		mov	ah,EMS_MAPPAGE
		mov	bx,[i]			// logical page
		mov	al,bl			// physical page
		mov	dx,[EMShandle]	// handle
		int	EMS_INT
		or	ah,ah
		jnz	error
		}
	}

	return;

error:
	error = _AH;
	strcpy (str,"MML_MapEMS: EMS error 0x");
	itoa(error,str2,16);
	strcpy (str,str2);
	Quit (str);
}

//==========================================================================

/*
======================
=
= MML_CheckForXMS
=
= Check for XMM driver
=
=======================
*/

boolean MML_CheckForXMS (void)
{
	numUMBs = 0;

asm {
	mov	ax,0x4300
	int	0x2f				// query status of installed diver
	cmp	al,0x80
	je	good
	}
	return false;
good:
	return true;
}


/*
======================
=
= MML_SetupXMS
=
= Try to allocate all upper memory block
=
=======================
*/

void MML_SetupXMS (void)
{
	unsigned	base,size;

asm	{
	mov	ax,0x4310
	int	0x2f
	mov	[WORD PTR XMSaddr],bx
	mov	[WORD PTR XMSaddr+2],es		// function pointer to XMS driver
	}

getmemory:
asm	{
	mov	ah,XMS_ALLOCUMB
	mov	dx,0xffff					// try for largest block possible
	call	[DWORD PTR XMSaddr]
	or	ax,ax
	jnz	gotone

	cmp	bl,0xb0						// error: smaller UMB is available
	jne	done;

	mov	ah,XMS_ALLOCUMB
	call	[DWORD PTR XMSaddr]		// DX holds largest available UMB
	or	ax,ax
	jz	done						// another error...
	}

gotone:
asm	{
	mov	[base],bx
	mov	[size],dx
	}
	MM_UseSpace (base,size);
	mminfo.XMSmem += size*16;
	UMBbase[numUMBs] = base;
	numUMBs++;
	if (numUMBs < MAXUMBS)
		goto getmemory;

done:;
}


/*
======================
=
= MML_ShutdownXMS
=
======================
*/

void MML_ShutdownXMS (void)
{
	int	i;
	unsigned	base;

	for (i=0;i<numUMBs;i++)
	{
		base = UMBbase[i];

asm	mov	ah,XMS_FREEUMB
asm	mov	dx,[base]
asm	call	[DWORD PTR XMSaddr]
	}
}

//==========================================================================

/*
======================
=
= MM_UseSpace
=
= Marks a range of paragraphs as usable by the memory manager
= This is used to mark space for the near heap, far heap, ems page frame,
= and upper memory blocks
=
======================
*/

void MM_UseSpace (unsigned segstart, unsigned seglength)
{
	int scan, last, mmnew;
	unsigned	oldend;
	long		extra;

	scan = last = 0;
	mmrover = 0;			// reset rover to start of memory

//
// search for the block that contains the range of segments
//
	while (mmblocks[scan].start+mmblocks[scan].length < segstart)
	{
		last = scan;
		scan = mmblocks[scan].next;
	}

//
// take the given range out of the block
//
	oldend = mmblocks[scan].start + mmblocks[scan].length;
	extra = oldend - (segstart+seglength);
	if (extra < 0)
		Quit ("MM_UseSpace: Segment spans two blocks!");

	if (segstart == mmblocks[scan].start)
	{
		mmblocks[last].next = mmblocks[scan].next;			// unlink block
		FREEBLOCK(scan);
		scan = last;
	}
	else
		mmblocks[scan].length = segstart-mmblocks[scan].start;	// shorten block

	if (extra > 0)
	{
		// GETNEWBLOCK
		mmnew = mmfree;
		mmfree = mmblocks[mmfree].next;
		
		mmblocks[mmnew].useptr = NULL;			// Keen addition

		mmblocks[mmnew].next = mmblocks[scan].next;
		mmblocks[scan].next = mmnew;
		mmblocks[mmnew].start = segstart+seglength;
		mmblocks[mmnew].length = extra;
		mmblocks[mmnew].attributes = LOCKBIT;
	}

}

//==========================================================================

/*
===================
=
= MM_Startup
=
= Grabs all space from turbo with malloc/farmalloc
= Allocates bufferseg misc buffer
=
===================
*/

static	char *ParmStrings[] = {"noems","noxms",""};

void MM_Startup (void)
{
	int i;
	unsigned 	long length;
	void far 	*start;
	unsigned 	segstart,seglength;

	if (mmstarted)
		MM_Shutdown ();


	mmstarted = true;
	bombonerror = true;
//
// set up the linked list (everything in the free list;
//
	for (i=0;i<MAXBLOCKS;i++)
		mmblocks[i].next = i+1;

//
// locked block of all memory until we punch out free space
//
	mmblocks[0].start = 0;
	mmblocks[0].length = 0xffff;
	mmblocks[0].attributes = LOCKBIT;
	mmblocks[0].next = MAXBLOCKS;
	mmrover = 0;
	mmfree = 1;


//
// get all available near conventional memory segments
//
	length=coreleft();
	start = (void far *)(nearheap = malloc(length));

	length -= 16-(FP_OFF(start)&15);
	length -= SAVENEARHEAP;
	seglength = length / 16;			// now in paragraphs
	segstart = FP_SEG(start)+(FP_OFF(start)+15)/16;
	MM_UseSpace (segstart,seglength);
	mminfo.nearheap = length;

//
// get all available far conventional memory segments
//
	length=farcoreleft();
	start = farheap = farmalloc(length);
	length -= 16-(FP_OFF(start)&15);
	length -= SAVEFARHEAP;
	seglength = length / 16;			// now in paragraphs
	segstart = FP_SEG(start)+(FP_OFF(start)+15)/16;
	MM_UseSpace (segstart,seglength);
	mminfo.farheap = length;
	mminfo.mainmem = mminfo.nearheap + mminfo.farheap;


//
// detect EMS and allocate up to 64K at page frame
//
	mminfo.EMSmem = 0;
	for (i = 1;i < _argc;i++)
	{
		if ( US_CheckParm(_argv[i],ParmStrings) == 0)
			goto emsskip;				// param NOEMS
	}

	if (MML_CheckForEMS())
	{
		MML_SetupEMS();					// allocate space
		MM_UseSpace (EMSpageframe,EMSpagesmapped*0x400);
		MML_MapEMS();					// map in used pages
		mminfo.EMSmem = EMSpagesmapped*0x4000l;
	}

//
// detect XMS and get upper memory blocks
//
emsskip:
	mminfo.XMSmem = 0;
	for (i = 1;i < _argc;i++)
	{
		if ( US_CheckParm(_argv[i],ParmStrings) == 0)	// BUG: NOXMS is index 1, not 0
			goto xmsskip;				// param NOXMS
	}

	if (MML_CheckForXMS())
		MML_SetupXMS();					// allocate as many UMBs as possible

//
// allocate the misc buffer
//
xmsskip:
	mmrover = 0;			// start looking for space after low block

	MM_GetPtr (&bufferseg,BUFFERSIZE);
}

//==========================================================================

/*
====================
=
= MM_Shutdown
=
= Frees all conventional, EMS, and XMS allocated
=
====================
*/

void MM_Shutdown (void)
{
  if (!mmstarted)
	return;

  farfree (farheap);
  free (nearheap);
  MML_ShutdownEMS ();
  MML_ShutdownXMS ();
}

//==========================================================================

/*
====================
=
= MM_GetPtr
=
= Allocates an unlocked, unpurgeable block
=
====================
*/

void MM_GetPtr (memptr *baseptr,unsigned long size)
{
	int 		scan, lastscan, endscan, purge, next, mmnew;
	int			search;
	unsigned	needed,startseg;

	needed = (size+15)/16;		// convert size from bytes to paragraphs

	// GETNEWBLOCK
	// fill in start and next after a spot is found
	if (mmfree == MAXBLOCKS)
	{
		// ClearBlock
		// We are out of blocks, so free a purgeable block
		scan = mmblocks[0].next;

		while (scan != MAXBLOCKS)
		{
			if (!ISLOCKED(scan) && ISPURGEABLE(scan))
			{
				MM_FreePtr(mmblocks[scan].useptr);
				break;
			}
			scan = mmblocks[scan].next;
		}

		if (scan == MAXBLOCKS)
			Quit ("MM_GetPtr: No purgeable blocks!");
	}

	mmnew = mmfree;
	mmfree = mmblocks[mmfree].next;

	mmblocks[mmnew].length = needed;
	mmblocks[mmnew].useptr = baseptr;
	mmblocks[mmnew].attributes = BASEATTRIBUTES;

	for (search = 0; search<3; search++)
	{
	//
	// first search:	try to allocate right after the rover, then on up
	// second search: 	search from the head pointer up to the rover
	// third search:	compress memory, then scan from start
		if (search == 1 && mmrover == 0)
			search++;

		switch (search)
		{
		case 0:
			lastscan = mmrover;
			scan = mmblocks[mmrover].next;
			endscan = MAXBLOCKS;
			break;
		case 1:
			lastscan = 0;
			scan = mmblocks[0].next;
			endscan = mmrover;
			break;
		case 2:
			MML_SortMem ();
			lastscan = 0;
			scan = mmblocks[0].next;
			endscan = MAXBLOCKS;
			break;
		}

		startseg = mmblocks[lastscan].start + mmblocks[lastscan].length;

		while (scan != endscan)
		{
			if (mmblocks[scan].start - startseg >= needed)
			{
			//
			// got enough space between the end of lastscan and
			// the start of scan, so throw out anything in the middle
			// and allocate the new block
			//
				purge = mmblocks[lastscan].next;
				mmblocks[lastscan].next = mmnew;
				mmblocks[mmnew].start = *(unsigned *)baseptr = startseg;
				mmblocks[mmnew].next = scan;
				while ( purge != scan)
				{	// free the purgeable block
					next = mmblocks[purge].next;
					FREEBLOCK(purge);
					purge = next;		// purge another if not at scan
				}
				mmrover = mmnew;
				return;	// good allocation!
			}

			//
			// if this block is not purgeable or locked, skip past it
			//
			if (ISLOCKED(scan) || !ISPURGEABLE(scan))
			{
				lastscan = scan;
				startseg = mmblocks[lastscan].start + mmblocks[lastscan].length;
			}


			scan=mmblocks[scan].next;		// look at next line
		}
	}

	if (bombonerror)
		Quit ("MM_GetPtr: Out of memory!");
	else
		mmerror = true;
}

//==========================================================================

/*
====================
=
= MM_FreePtr
=
= Deallocates an unlocked, purgeable block
=
====================
*/

void MM_FreePtr (memptr *baseptr)
{
	int scan, last;

	last = 0;
	scan = mmblocks[0].next;

	if (baseptr == mmblocks[mmrover].useptr)	// removed the last allocated block
		mmrover = 0;

	while (scan != MAXBLOCKS && mmblocks[scan].useptr != baseptr)
	{
		last = scan;
		scan = mmblocks[scan].next;
	}

	if (scan == MAXBLOCKS)
		Quit ("MM_FreePtr: Block not found!");

	mmblocks[last].next = mmblocks[scan].next;

	FREEBLOCK(scan);
}
//==========================================================================

void MML_SetRover(memptr *baseptr)
{
	int start;

	start = mmrover;

	do
	{
		if (mmblocks[mmrover].useptr == baseptr)
			break;

		mmrover = mmblocks[mmrover].next;

		if (mmrover == MAXBLOCKS)
			mmrover = 0;
		else if (mmrover == start)
			Quit ("MML_SetRover: Block not found!");

	} while (1);
}

/*
=====================
=
= MM_SetPurge
=
= Sets the purge attribute for a block (locked blocks cannot be made purgeable)
=
=====================
*/

void MM_SetPurge (memptr *baseptr, boolean purge)
{
	MML_SetRover(baseptr);
	mmblocks[mmrover].attributes &= ~PURGEBIT;
	mmblocks[mmrover].attributes |= purge*PURGEBIT;
}

//==========================================================================

/*
=====================
=
= MM_SetLock
=
= Locks / unlocks the block
=
=====================
*/

void MM_SetLock (memptr *baseptr, boolean locked)
{
	MML_SetRover(baseptr);
	mmblocks[mmrover].attributes &= ~LOCKBIT;
	mmblocks[mmrover].attributes |= locked*LOCKBIT;
}

//==========================================================================

/*
=====================
=
= MML_SortMem
=
= Throws out all purgeable stuff and compresses movable blocks
=
=====================
*/

void MML_SortMem (void)
{
	int			scan, last, next;
	unsigned	start,length,source,dest,oldborder;
	int			playing;

	//
	// lock down a currently playing sound
	//
	playing = SD_SoundPlaying ();
	if (playing)
	{
		switch (SoundMode)
		{
		case sdm_PC:
			playing += STARTPCSOUNDS;
			break;
		case sdm_AdLib:
			playing += STARTADLIBSOUNDS;
			break;
		}
		MM_SetLock(&(memptr)audiosegs[playing],true);
	}


	SD_StopSound();
	oldborder = bordercolor;
	VW_ColorBorder (15);

	scan = 0;

	last = MAXBLOCKS;		// shut up compiler warning

	while (scan != MAXBLOCKS)
	{
		if (ISLOCKED(scan))
		{
		//
		// block is locked, so try to pile later blocks right after it
		//
			start = mmblocks[scan].start + mmblocks[scan].length;
		}
		else if (ISPURGEABLE(scan))
		{
		//
		// throw out the purgeable block
		//
			next = mmblocks[scan].next;
			FREEBLOCK(scan);
			mmblocks[last].next = next;
			scan = next;
			continue;
		}
		else
		{
		//
		// push the non purgeable block on top of the last moved block
		//
			if (mmblocks[scan].start != start)
			{
				length = mmblocks[scan].length;
				source = mmblocks[scan].start;
				dest = start;
				while (length > 0xf00)
				{
					movedata(source,0,dest,0,0xf00*16);
					length -= 0xf00;
					source += 0xf00;
					dest += 0xf00;
				}
				movedata(source,0,dest,0,length*16);

				mmblocks[scan].start = start;
				*(unsigned *)mmblocks[scan].useptr = start;
			}
			start = mmblocks[scan].start + mmblocks[scan].length;
		}

		last = scan;
		scan = mmblocks[scan].next;		// go to next block
	}

	mmrover = 0;

	VW_ColorBorder (oldborder);

	if (playing)
		MM_SetLock(&(memptr)audiosegs[playing],false);
}


//==========================================================================
#if GRMODE == EGAGR
/*
=====================
=
= MM_ShowMemory
=
=====================
*/

void MM_ShowMemory (void)
{
	int scan;
	unsigned color,temp;
	long	end,owner;
	char    scratch[80],str[10];

	VW_SetDefaultColors();
	VW_SetLineWidth(40);
	temp = bufferofs;
	bufferofs = 0;
	VW_SetScreen (0,0);

	scan = 0;

	end = -1;

//CA_OpenDebug ();

	while (scan != MAXBLOCKS)
	{
		if (ISPURGEABLE(scan))
			color = 5;		// dark purple = purgeable
		else
			color = 9;		// medium blue = non purgeable
		if (ISLOCKED(scan))
			color = 12;		// red = locked
		if (mmblocks[scan].start<=end)
			Quit ("MM_ShowMemory: Memory block order corrupted!");
		end = mmblocks[scan].start+mmblocks[scan].length-1;
		VW_Hlin(mmblocks[scan].start,(unsigned)end,0,color);
		VW_Plot(mmblocks[scan].start,0,15);
		if (mmblocks[mmblocks[scan].next].start > end+1)
			VW_Hlin(end+1,mmblocks[mmblocks[scan].next].start,0,0);	// black = free

#if 0
strcpy (scratch,"Size:");
ltoa ((long)mmblocks[scan].length*16,str,10);
strcat (scratch,str);
strcat (scratch,"\tOwner:0x");
owner = (unsigned)mmblocks[scan].useptr;
ultoa (owner,str,16);
strcat (scratch,str);
strcat (scratch,"\n");
write (debughandle,scratch,strlen(scratch));
#endif

		scan = mmblocks[scan].next;
	}

//CA_CloseDebug ();

	IN_Ack();
	VW_SetLineWidth(64);
	bufferofs = temp;
}
#endif
//==========================================================================


/*
======================
=
= MM_FreeMemory
=
= Returns the total free space with or without purging
=
======================
*/

long MM_FreeMemory (boolean withpurging)
{
	unsigned free = 0;
	int scan = 0;

	while (mmblocks[scan].next != MAXBLOCKS)
	{
		if (withpurging && ISPURGEABLE(scan) && !ISLOCKED(scan))
			free += mmblocks[scan].length;
		free += mmblocks[mmblocks[scan].next].start - (mmblocks[scan].start + mmblocks[scan].length);
		scan = mmblocks[scan].next;
	}

	return free*16l;
}

//==========================================================================

/*
=====================
=
= MM_BombOnError
=
=====================
*/

void MM_BombOnError (boolean bomb)
{
	bombonerror = bomb;
}


