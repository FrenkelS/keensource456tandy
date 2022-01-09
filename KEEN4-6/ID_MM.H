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

// ID_MM.H

#ifndef __ID_CA__

#define __ID_CA__


//==========================================================================

typedef void _seg * memptr;

typedef struct
{
	long	EMSmem,XMSmem,mainmem;
} mminfotype;

//==========================================================================

extern	mminfotype	mminfo;
extern	boolean		mmerror;

//==========================================================================

void MM_UseSpace (unsigned segstart, unsigned seglength);

void MM_Startup (void);
void MM_Shutdown (void);

void MM_GetPtr (memptr *baseptr,unsigned long size);
void MM_FreePtr (memptr *baseptr);

void MM_SetPurge (memptr *baseptr, boolean purge);
void MM_SetLock (memptr *baseptr, boolean locked);

#if GRMODE == EGAGR
void MM_ShowMemory (void);
#endif

long MM_FreeMemory (boolean withpurging);

void MM_BombOnError (boolean bomb);

#endif