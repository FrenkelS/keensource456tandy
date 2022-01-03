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

//
//	ID Engine
//	ID_SD.h - Sound Manager Header
//	v1.0d1
//	By Jason Blochowiak
//

#ifndef	__TYPES__
#include "ID_Types.h"
#endif

#ifndef	__ID_SD__
#define	__ID_SD__

#ifdef	__DEBUG__
#define	__DEBUG_SoundMgr__
#endif

#define	TickBase	70		// 70Hz per tick - used as a base for timer 0

typedef	enum	{
					sdm_Off,
					sdm_PC,sdm_AdLib,
				}	SDMode;
typedef	enum	{
					smm_Off,smm_AdLib,smm_Tandy
				}	SMMode;

#if 1
typedef	struct
		{
			word	length,
					values[1];
		} MusicGroup;
#else
typedef	struct
		{
			word	flags,
					count,
					offsets[1];
		} MusicGroup;
#endif


// Global variables
extern	boolean		AdLibPresent,
					QuietFX;
extern	SDMode		SoundMode;
extern	SMMode		MusicMode;
extern	longword	TimeCount;					// Global time in ticks

// Function prototypes
extern	void	SD_Startup(void),
				SD_Shutdown(void),
				SD_Default(boolean gotit,SDMode sd,SMMode sm),
				SD_PlaySound(soundnames sound),
				SD_StopSound(void),
				SD_WaitSoundDone(void),
				SD_StartMusic(MusicGroup far *music),
				SD_MusicOn(void),
				SD_MusicOff(void),
				SD_SetSoundMode(SDMode mode),
				SD_SetMusicMode(SMMode mode);
extern	word	SD_SoundPlaying(void);

#endif
