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

// ID_RF.H

#define __ID_RF__

#ifndef __ID_MM__
#include "ID_MM.H"
#endif

/*
=============================================================================

							CONSTANTS

=============================================================================
*/

#define	MINTICS				2
#define	MAXTICS				5
#define DEMOTICS			3

#define	MAPBORDER			2		// map border must be at least 1

#ifdef KEEN5

#define	MAXSPRITES			60		// max tracked sprites
#define	MAXANIMTILES		90		// max animating tiles on screen
#define MAXANIMTYPES		80		// max different unique anim tiles on map

#define	MAXMAPHEIGHT		250

#else

#define	MAXSPRITES			60		// max tracked sprites
#define	MAXANIMTILES		90		// max animating tiles on screen
#define MAXANIMTYPES		65		// max different unique anim tiles on map

#define	MAXMAPHEIGHT		200

#endif

#define	PRIORITIES			4
#define	MASKEDTILEPRIORITY	3		// planes go: 0,1,2,MTILES,3

#define TILEGLOBAL			256
#define PIXGLOBAL			16

#define	G_T_SHIFT			8		// global >> ?? = tile
#define	G_P_SHIFT			4		// global >> ?? = pixels
#define P_T_SHIFT			4		// pixels >> ?? = tile

#if GRMODE == EGAGR || GRMODE == CGAGR
#define	PORTTILESWIDE		21      // all drawing takes place inside a
#define	PORTTILESHIGH		14		// non displayed port of this size
#elif GRMODE == TGAGR
#define	PORTTILESWIDE		21      // all drawing takes place inside a
#define	PORTTILESHIGH		12		// non displayed port of this size
#endif

#define UPDATEWIDE			(PORTTILESWIDE+1)
#define UPDATEHIGH			PORTTILESHIGH

/*
=============================================================================

						 PUBLIC VARIABLES

=============================================================================
*/


extern	boolean		compatibility;			// crippled refresh for weirdo SVGAs

extern	unsigned	tics;
extern	long		lasttimecount;

extern	unsigned	originxglobal,originyglobal;
extern	unsigned	originxtile,originytile;
extern	unsigned	originxscreen,originyscreen;

extern	unsigned	mapwidth,mapheight,mapbyteswide,mapwordswide
					,mapbytesextra,mapwordsextra;
extern	unsigned	mapbwidthtable[MAXMAPHEIGHT];

extern	unsigned	originxmin,originxmax,originymin,originymax;

extern	unsigned	masterofs;

//
// the floating update window is also used by the view manager for
// double buffer tracking
//

extern	byte		*updateptr;				// current start of update window

#if GRMODE == CGAGR || GRMODE == TGAGR
extern	byte		*baseupdateptr;
#endif

extern unsigned	blockstarts[UPDATEWIDE*UPDATEHIGH];
extern unsigned	updatemapofs[UPDATEWIDE*UPDATEHIGH];
extern unsigned	uwidthtable[UPDATEHIGH];		// lookup instead of multiply

#define	UPDATETERMINATE	0x0301

/*
=============================================================================

						 PUBLIC FUNCTIONS

=============================================================================
*/

void RF_Startup (void);
void RF_Shutdown (void);

void RF_FixOfs (void);
void RF_NewMap (void);
void RF_MarkTileGraphics (void);
void RF_SetScrollBlockHorizontal (int y);
void RF_SetScrollBlockVertical (int x);
void RF_NewPosition (unsigned x, unsigned y);
void RF_Scroll (int x, int y);

void RF_MapToMap (unsigned srcx, unsigned srcy,
				  unsigned destx, unsigned desty,
				  unsigned width, unsigned height);
void RF_MemToMap (unsigned far *source, unsigned plane,
				  unsigned destx, unsigned desty,
				  unsigned width, unsigned height);

void RF_PlaceSprite (void **user,unsigned globalx,unsigned globaly,
	unsigned spritenumber, int priority);
void RF_RemoveSprite (void **user);

void RF_Refresh (void);
void RF_ForceRefresh (void);
void RF_SetRefreshHook (void (*func) (void) );

#if GRMODE == EGAGR
unsigned RF_FindFreeBuffer (void);
#endif
