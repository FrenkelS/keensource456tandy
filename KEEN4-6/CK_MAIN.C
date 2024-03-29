/* Commander Keen 4 Tandy Version Source Code
 * Copyright (C) 2021-2022 Frenkel Smeijers
 *
 * This file is primarily based on:
 * Reconstructed Commander Keen 4-6 Source Code
 * Copyright (C) 2021 K1n9_Duk3
 *
 * This file is loosely based on:
 * Keen Dreams Source Code
 * Copyright (C) 2014 Javier M. Chavez
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

// CK_MAIN.C
/*
=============================================================================

							COMMANDER KEEN

					An Id Software production

=============================================================================
*/

#include "CK_DEF.H"

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

boolean tedlevel;
Uint16 tedlevelnum;

char str[80], str2[20];
boolean storedemo, jerk;

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

//===========================================================================

/*
=====================
=
= SizeText
=
= Calculates height of a string that contains line breaks
=
=====================
*/

Uint16 SizeText(char *text)
{
	register char *ptr;
	char c;
	Uint16 w, h;
	char strbuf[80];
	Uint16 height = 0;

	ptr = &strbuf[0];
	while ((c=*(text++)) != '\0')
	{
		*(ptr++) = c;
		if (c == '\n' || !*text)
		{
			USL_MeasureString(strbuf, &w, &h);	// BUG: strbuf may not have a terminating '\0' at the end!
			height += h;
			ptr = &strbuf[0];
		}
	}
	return height;
}

//===========================================================================

/*
==========================
=
= ShutdownId
=
= Shuts down all ID_?? managers
=
==========================
*/

void ShutdownId(void)
{
	US_Shutdown();
	SD_Shutdown();
	IN_Shutdown();
	RF_Shutdown();
	VW_Shutdown();
	CA_Shutdown();
	MM_Shutdown();
}


//===========================================================================

/*
==========================
=
= InitGame
=
= Load a few things right away
=
==========================
*/

void InitGame(void)
{
	static char *ParmStrings[] = {"JERK", ""};

	Uint16 segstart,seglength;
	Sint16 i;

	// Note: The value of the jerk variable is replaced with the value
	// read from the config file during US_Startup, which means the
	// JERK parameter has absolutely no effect if a valid config file
	// exists. The parameter check should be moved to some place after
	// US_Startup to make it work reliably.
	
	for (i=1; i < _argc; i++)
	{
		if (US_CheckParm(_argv[i], ParmStrings) == 0)
		{
			jerk = true;
		}
	}

	US_TextScreen();

	MM_Startup();
	VW_Startup();
	RF_Startup();
	IN_Startup();
	SD_Startup();
	US_Startup();

	US_UpdateTextScreen();

	CA_Startup();
	US_Setup();

	US_SetLoadSaveHooks(&LoadTheGame, &SaveTheGame, &ResetGame);
	drawcachebox = DialogDraw;
	updatecachebox = DialogUpdate;
	finishcachebox = DialogFinish;

//
// load in and lock down some basic chunks
//

	CA_ClearMarks();

	CA_MarkGrChunk(STARTFONT);
	CA_MarkGrChunk(STARTTILE8);
	CA_MarkGrChunk(STARTTILE8M);
#if GRMODE == EGAGR
	CA_MarkGrChunk(CORDPICM);
	CA_MarkGrChunk(METALPOLEPICM);
#endif

	CA_CacheMarks(NULL);

	MM_SetLock(&grsegs[STARTFONT], true);
	MM_SetLock(&grsegs[STARTTILE8], true);
	MM_SetLock(&grsegs[STARTTILE8M], true);
#if GRMODE == EGAGR
	MM_SetLock(&grsegs[CORDPICM], true);
	MM_SetLock(&grsegs[METALPOLEPICM], true);
#endif

	fontcolor = WHITE;

	US_FinishTextScreen();

//
// reclaim the memory from the linked in text screen
//
	segstart = FP_SEG(&introscn);
	seglength = 4000/16;
	if (FP_OFF(&introscn))
	{
		segstart++;
		seglength--;
	}
	MM_UseSpace (segstart,seglength);
	mminfo.mainmem += 4000;

	VW_SetScreenMode(GRMODE);
#if GRMODE == CGAGR
	VW_ColorBorder(BROWN);
#elif GRMODE == EGAGR || GRMODE == TGAGR
	VW_ColorBorder(CYAN);
#endif
}

//===========================================================================

/*
==========================
=
= Quit
=
==========================
*/

void Quit(char *error)
{
	Uint16 finscreen;

	if (!error)
	{
		CA_SetAllPurge();
		CA_CacheGrChunk(ORDERSCREEN);
		finscreen = (Uint16)grsegs[ORDERSCREEN];
	}

	// BUG: VW_ClearVideo may brick the system if screenseg is 0
	// (i.e. VW_SetScreenMode has not been executed) - this may
	// happen if the code runs into an error during InitGame
	// (EMS/XMS errors, files not found etc.)
	VW_ClearVideo();
	VW_SetLineWidth(40);

	ShutdownId();
	if (error && *error)
	{
		puts(error);
		if (tedlevel)
		{
			getch();
			execlp("TED5.EXE", "TED5.EXE", "/LAUNCH", NULL);
		}
		else if (US_ParmPresent("windows"))
		{
			bioskey(0);
		}
		exit(1);
	}

	if (!NoWait)
	{
		movedata(finscreen, 7, 0xB800, 0, 4000);
		gotoxy(1, 24);
		if (US_ParmPresent("windows"))
		{
			bioskey(0);
		}
	}

	exit(0);
}

//===========================================================================

/*
==================
=
= TEDDeath
=
==================
*/

void TEDDeath(void)
{
	ShutdownId();
	execlp("TED5.EXE", "TED5.EXE", "/LAUNCH", NULL);
	// BUG: should call exit(1); here in case starting TED5 fails
}

//===========================================================================

/*
=====================
=
= DemoLoop
=
=====================
*/

void DemoLoop(void)
{
	static char *ParmStrings[] = {"easy", "normal", "hard", ""};

	register Sint16 i, state;
	Sint16 level;

//
// check for launch from ted
//
	if (tedlevel)
	{
		NewGame();
		CA_LoadAllSounds();
		gamestate.mapon = tedlevelnum;
		restartgame = gd_Normal;
		for (i = 1;i < _argc;i++)
		{
			if ( (level = US_CheckParm(_argv[i],ParmStrings)) == -1)
				continue;

			restartgame = level+gd_Easy;
			break;
		}
		GameLoop();
		TEDDeath();
	}

//
// demo loop
//
	state = 0;
	playstate = ex_stillplaying;
	while (1)
	{
		switch (state++)
		{
		case 0:
#if GRMODE == CGAGR || GRMODE == TGAGR
			ShowTitle();
#elif GRMODE == EGAGR
			if (nopan)
			{
				ShowTitle();
			}
			else
			{
				Terminator();
			}
#endif
			break;

		case 1:
			RunDemo(0);
			break;

		case 2:
#if GRMODE == CGAGR || GRMODE == TGAGR
			ShowCredits();
#elif GRMODE == EGAGR
			StarWars();
#endif
			break;

		case 3:
			RunDemo(1);
			break;

		case 4:
			ShowHighScores();
			break;

		case 5:
			RunDemo(2);
			break;

		case 6:
			state = 0;
			RunDemo(3);
			break;
		}

		while (playstate == ex_resetgame || playstate == ex_loadedgame)
		{
			GameLoop();
			ShowHighScores();
			if (playstate == ex_resetgame || playstate == ex_loadedgame)
			{
				continue;	// don't show title screen, go directly to GameLoop();
			}
			ShowTitle();
		}
	}
}

//===========================================================================



/*
==========================
=
= main
=
==========================
*/

void main(void)
{
	if (US_ParmPresent("DEMO"))
		storedemo = true;

	InitGame();

	if (NoWait || tedlevel)
		debugok = true;

	DemoLoop();
	Quit("Demo loop exited???");
}