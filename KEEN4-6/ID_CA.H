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

// ID_CA.H

#ifndef __TYPES__
#include "ID_TYPES.H"
#endif

#ifndef __ID_MM__
#include "ID_MM.H"
#endif

#ifndef __ID_GLOB__
#include "ID_GLOB.H"
#endif

#define __ID_CA__

//===========================================================================

//#define NOMAPS
//#define NOGRAPHICS
//#define NOAUDIO

#define MAPHEADERLINKED
#define GRHEADERLINKED
#define AUDIOHEADERLINKED

#define NUMMAPS		30
#define MAPPLANES	3

//
// tile info defines, as bytes after tinf the table starts
//


//
// TILEINFO offsets
//
#define SPEED		402
#define ANIM		(SPEED+NUMTILE16)

//
// TILEINFOM offsets
//
#define NORTHWALL		(ANIM+NUMTILE16)
#define EASTWALL		(NORTHWALL+NUMTILE16M)
#define SOUTHWALL		(EASTWALL+NUMTILE16M)
#define WESTWALL		(SOUTHWALL+NUMTILE16M)
#define MANIM		(WESTWALL+NUMTILE16M)
#define INTILE		(MANIM+NUMTILE16M)
#define MSPEED		(INTILE+NUMTILE16M)

//===========================================================================

typedef	struct
{
	long		planestart[MAPPLANES];
	unsigned	planelength[MAPPLANES];
	unsigned	width,height;
	char		name[16];
} maptype;

//===========================================================================

extern	byte 		_seg	*tinf;
extern	int			mapon;

extern	unsigned	_seg	*mapsegs[MAPPLANES];
extern	maptype		_seg	*mapheaderseg[NUMMAPS];
extern	byte		_seg	*audiosegs[NUMSNDCHUNKS];
extern	void		_seg	*grsegs[NUMCHUNKS];

extern	byte		far	grneeded[NUMCHUNKS];
extern	byte		ca_levelbit,ca_levelnum;

extern	int			profilehandle,debughandle;

//
// hooks for custom cache dialogs
//
extern	void	(*drawcachebox)		(char *title, unsigned numcache);
extern	void	(*updatecachebox)	(void);
extern	void	(*finishcachebox)	(void);

//===========================================================================

// just for the score box reshifting

void CA_ShiftSprite (unsigned segment,unsigned source,unsigned dest,
	unsigned width, unsigned height, unsigned pixshift);

//===========================================================================

void CA_OpenDebug (void);
void CA_CloseDebug (void);
boolean CA_FarRead (int handle, byte far *dest, long length);
boolean CA_FarWrite (int handle, byte far *source, long length);

long CA_RLEWCompress (unsigned huge *source, long length, unsigned huge *dest,
  unsigned rlewtag);

void CA_RLEWexpand (unsigned huge *source, unsigned huge *dest,long length,
  unsigned rlewtag);

void CA_Startup (void);
void CA_Shutdown (void);

void CA_CacheAudioChunk (int chunk);
void CA_LoadAllSounds (void);

void CA_UpLevel (void);
void CA_DownLevel (void);

void CA_SetAllPurge (void);

void CA_ClearMarks (void);

#define CA_MarkGrChunk(chunk)	grneeded[chunk]|=ca_levelbit
#define CA_UnmarkGrChunk(chunk)	grneeded[chunk]&=~ca_levelbit

void CA_CacheGrChunk (int chunk);
void CA_CacheMap (int mapnum);

void CA_CacheMarks (char *title);

