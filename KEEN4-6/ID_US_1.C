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
//      ID Engine
//      ID_US_1.c - User Manager - General routines
//      v1.1d1
//      By Jason Blochowiak
//      Hacked up for Catacomb 3D
//

//
//      This module handles dealing with user input & feedback
//
//      Depends on: Input Mgr, View Mgr, some variables from the Sound, Caching,
//              and Refresh Mgrs, Memory Mgr for background save/restore
//
//      Globals:
//              ingame - Flag set by game indicating if a game is in progress
//      abortgame - Flag set if the current game should be aborted (if a load
//                      game fails)
//              loadedgame - Flag set if a game was loaded
//              restartgame - Normally set to gd_Continue, this is set to one of the
//                      difficulty levels if a new game should be started
//              PrintX, PrintY - Where the User Mgr will print (global coords)
//              WindowX,WindowY,WindowW,WindowH - The dimensions of the current
//                      window
//

#include "ID_HEADS.H"

#pragma hdrstop

#pragma warn    -pia


//      Special imports
extern  boolean         showscorebox;
extern	boolean		jerk;
extern  boolean         oldshooting;
extern  ScanCode        firescan;

//      Global variables
		boolean         NoWait,
					HighScoresDirty;
		word            PrintX,PrintY;
		word            WindowX,WindowY,WindowW,WindowH;

//      Internal variables
#define ConfigVersion   4

static  char            *ParmStrings[] = {"TEDLEVEL","NOWAIT"},
					*ParmStrings2[] = {"COMP","NOCOMP"};
static  boolean         US_Started;
static	char            *abortprogram;	// Set to error msg if program is dying

		void            (*USL_MeasureString)(char far *,word *,word *) = VW_MeasurePropString,
					(*USL_DrawString)(char far *) = VWB_DrawPropString;

		boolean         (*USL_SaveGame)(int),(*USL_LoadGame)(int);
		void            (*USL_ResetGame)(void);
		SaveGame        Games[MaxSaveGames];
		HighScore       Scores[MaxScores] =
					{
#if defined GOODTIMES
						{"Id Software",10000,0},
						{"Adrian Carmack",10000,0},
						{"John Carmack",10000,0},
						{"Kevin Cloud",10000,0},
						{"Shawn Green",10000,0},
						{"Tom Hall",10000,0},
						{"John Romero",10000,0},
						{"Jay Wilbur",10000,0},
#else
						{"Id Software - '91",10000,0},
						{"",10000,0},
						{"Jason Blochowiak",10000,0},
						{"Adrian Carmack",10000,0},
						{"John Carmack",10000,0},
						{"Tom Hall",10000,0},
						{"John Romero",10000,0},
						{"",10000,0},
#endif
					};

//      Internal routines
void	USL_ClearWindow(void);
void	USL_SaveWindow(WindowRec *win);
void	USL_RestoreWindow(WindowRec *win);

//      Public routines

///////////////////////////////////////////////////////////////////////////
//
//      USL_HardError() - Handles the Abort/Retry/Fail sort of errors passed
//                      from DOS.
//
///////////////////////////////////////////////////////////////////////////
#pragma warn    -par
#pragma warn    -rch
int
USL_HardError(word errval,int ax,int bp,int si)
{
#define IGNORE  0
#define RETRY   1
#define ABORT   2
extern  void    ShutdownId(void);

static  char            buf[32];
static  WindowRec       wr;
		int                     di;
		char            c,*s,*t;


	di = _DI;

	if (ax < 0)
		s = "Device Error";
	else
	{
		if ((di & 0x00ff) == 0)
			s = "Drive ~ is Write Protected";
		else
			s = "Error on Drive ~";
		for (t = buf;*s;s++,t++)        // Can't use sprintf()
			if ((*t = *s) == '~')
				*t = (ax & 0x00ff) + 'A';
		*t = '\0';
		s = buf;
	}

	c = peekb(0x40,0x49);   // Get the current screen mode
	if ((c < 4) || (c == 7))
		goto oh_kill_me;

	// DEBUG - handle screen cleanup

	USL_SaveWindow(&wr);
	US_CenterWindow(30,3);
	US_CPrint(s);
	US_CPrint("(R)etry or (A)bort?");
	VW_UpdateScreen();
	IN_ClearKeysDown();

asm     sti     // Let the keyboard interrupts come through

	while (true)
	{
		switch (IN_WaitForASCII())
		{
		case key_Escape:
		case 'a':
		case 'A':
			goto oh_kill_me;
		case key_Return:
		case key_Space:
		case 'r':
		case 'R':
			USL_ClearWindow();
			VW_UpdateScreen();
			USL_RestoreWindow(&wr);
			return(RETRY);
		}
	}

oh_kill_me:
	abortprogram = s;
	ShutdownId();
	fprintf(stderr,"Terminal Error: %s\n",s);
	if (tedlevel)
		fprintf(stderr,"You launched from TED. I suggest that you reboot...\n");

	return(ABORT);
#undef  IGNORE
#undef  RETRY
#undef  ABORT
}
#pragma warn    +par
#pragma warn    +rch

///////////////////////////////////////////////////////////////////////////
//
//      USL_GiveSaveName() - Returns a pointer to a static buffer that contains
//              the filename to use for the specified save game
//
///////////////////////////////////////////////////////////////////////////
char *
USL_GiveSaveName(word game)
{
static  char    name[] = "SAVEGAMx."EXTENSION;

	name[7] = '0' + game;
	return(name);
}

///////////////////////////////////////////////////////////////////////////
//
//      US_SetLoadSaveHooks() - Sets the routines that the User Mgr calls after
//              reading or writing the save game headers
//
///////////////////////////////////////////////////////////////////////////
void
US_SetLoadSaveHooks(boolean (*load)(int),boolean (*save)(int),void (*reset)(void))
{
	USL_LoadGame = load;
	USL_SaveGame = save;
	USL_ResetGame = reset;
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_ReadConfig() - Reads the configuration file, if present, and sets
//              things up accordingly. If it's not present, uses defaults. This file
//              includes the high scores.
//
///////////////////////////////////////////////////////////////////////////
static void
USL_ReadConfig(void)
{
	boolean         gotit, hadAdLib;
	char            sig[sizeof(EXTENSION)];
	word            version;
	int                     file;
	SDMode          sd;
	SMMode          sm;
	ControlType     ctl;

	if ((file = open("CONFIG."EXTENSION,O_BINARY | O_RDONLY)) != -1)
	{
		read(file,sig,sizeof(EXTENSION));
		read(file,&version,sizeof(version));
		if (strcmp(sig,EXTENSION) || (version != ConfigVersion))
		{
			close(file);
			goto rcfailed;
		}
		read(file,Scores,sizeof(HighScore) * MaxScores);
		read(file,&sd,sizeof(sd));
		read(file,&sm,sizeof(sm));
		read(file,&ctl,sizeof(ctl));
		read(file,&(KbdDefs[0]),sizeof(KbdDefs[0]));
		read(file,&showscorebox,sizeof(showscorebox));
		read(file,&compatibility,sizeof(compatibility));
		read(file,&QuietFX,sizeof(QuietFX));
		read(file,&hadAdLib,sizeof(hadAdLib));
		read(file,&jerk,sizeof(jerk));
		read(file,&oldshooting,sizeof(oldshooting));
		read(file,&firescan,sizeof(firescan));
		read(file,&GravisGamepad,sizeof(GravisGamepad));
		read(file,&GravisMap,sizeof(GravisMap));
		close(file);

		HighScoresDirty = false;
		gotit = true;
	}
	else
	{
rcfailed:
		sd = sdm_Off;
		sm = smm_Off;
		ctl = ctrl_Keyboard;
		showscorebox = false;
		oldshooting = false;
		gotit = false;
		HighScoresDirty = true;
	}

	SD_Default(gotit && (hadAdLib==AdLibPresent), sd,sm);
	IN_Default(gotit,ctl);
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_WriteConfig() - Writes out the current configuration, including the
//              high scores.
//
///////////////////////////////////////////////////////////////////////////
static void
USL_WriteConfig(void)
{
	word    version;
	int             file;

	version = ConfigVersion;
	file = open("CONFIG."EXTENSION,O_CREAT | O_BINARY | O_WRONLY,
				S_IREAD | S_IWRITE | S_IFREG);
	if (file != -1)
	{
		write(file,EXTENSION,sizeof(EXTENSION));
		write(file,&version,sizeof(version));
		write(file,Scores,sizeof(HighScore) * MaxScores);
		write(file,&SoundMode,sizeof(SoundMode));
		write(file,&MusicMode,sizeof(MusicMode));
		write(file,&(Controls[0]),sizeof(Controls[0]));
		write(file,&(KbdDefs[0]),sizeof(KbdDefs[0]));
		write(file,&showscorebox,sizeof(showscorebox));
		write(file,&compatibility,sizeof(compatibility));
		write(file,&QuietFX,sizeof(QuietFX));
		write(file,&AdLibPresent,sizeof(AdLibPresent));
		write(file,&jerk,sizeof(jerk));
		write(file,&oldshooting,sizeof(oldshooting));
		write(file,&firescan,sizeof(firescan));
		write(file,&GravisGamepad,sizeof(GravisGamepad));
		write(file,&GravisMap,sizeof(GravisMap));
		close(file);
	}
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_CheckSavedGames() - Checks to see which saved games are present
//              & valid
//
///////////////////////////////////////////////////////////////////////////
void
USL_CheckSavedGames(void)
{
	boolean         ok;
	char            *filename;
	word            i;
	int                     file;
	SaveGame        *game;

	for (i = 0,game = Games;i < MaxSaveGames;i++,game++)
	{
		filename = USL_GiveSaveName(i);
		ok = false;
		if ((file = open(filename,O_BINARY | O_RDONLY)) != -1)
		{
			if
			(
				(read(file,game,sizeof(*game)) == sizeof(*game))
			&&      (!strcmp(game->signature,EXTENSION))
			&&      (game->oldtest == &PrintX)
			)
				ok = true;

			close(file);
		}

		if (ok)
			game->present = true;
		else
		{
			strcpy(game->signature,EXTENSION);
			game->present = false;
			strcpy(game->name,"Empty");
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//      US_Startup() - Starts the User Mgr
//
///////////////////////////////////////////////////////////////////////////
void
US_Startup(void)
{
	int     i;

	if (US_Started)
		return;

	harderr(USL_HardError); // Install the fatal error handler

	US_InitRndT(true);              // Initialize the random number generator

	USL_ReadConfig();               // Read config file

	for (i = 1;i < _argc;i++)
	{
		switch (US_CheckParm(_argv[i],ParmStrings2))
		{
		case 0:
#if GRMODE == EGAGR
			compatibility = true;
#endif
			break;
		case 1:
			compatibility = false;
			break;
		}
	}

	US_Started = true;
}

///////////////////////////////////////////////////////////////////////////
//
//      US_Setup() - Does the disk access part of the User Mgr's startup
//
///////////////////////////////////////////////////////////////////////////
void
US_Setup(void)
{
	USL_SaveGame = 0;
	USL_LoadGame = 0;
	USL_CheckSavedGames();  // Check which saved games are present
}

///////////////////////////////////////////////////////////////////////////
//
//      US_Shutdown() - Shuts down the User Mgr
//
///////////////////////////////////////////////////////////////////////////
void
US_Shutdown(void)
{
	if (!US_Started)
		return;

	if (!abortprogram)
		USL_WriteConfig();

	US_Started = false;
}

///////////////////////////////////////////////////////////////////////////
//
//      US_CheckParm() - checks to see if a string matches one of a set of
//              strings. The check is case insensitive. The routine returns the
//              index of the string that matched, or -1 if no matches were found
//
///////////////////////////////////////////////////////////////////////////
int
US_CheckParm(char *parm,char **strings)
{
	char    cp,cs,
			*p,*s;
	int             i;

	while (!isalpha(*parm)) // Skip non-alphas
		parm++;

	for (i = 0;*strings && **strings;i++)
	{
		for (s = *strings++,p = parm,cs = cp = 0;cs == cp;)
		{
			cs = *s++;
			if (!cs)
				return(i);
			cp = *p++;

			if (isupper(cs))
				cs = tolower(cs);
			if (isupper(cp))
				cp = tolower(cp);
		}
	}
	return(-1);
}

///////////////////////////////////////////////////////////////////////////
//
//      US_ParmPresent() - checks if a given string was passed as a command
//              line parameter at startup
//
///////////////////////////////////////////////////////////////////////////
boolean
US_ParmPresent(char *arg)
{
	int i;
	char *strings[2];

	strings[0] = arg;
	strings[1] = NULL;

	for (i=1; i<_argc; i++)
	{
		if (US_CheckParm(_argv[i], strings) != -1)
			return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_ScreenDraw() - Draws a chunk of the text screen (called only by
//              US_TextScreen())
//
///////////////////////////////////////////////////////////////////////////
static void
USL_ScreenDraw(word x,word y,char *s,byte attr)
{
	byte    far *screen,far *oscreen;

	screen = MK_FP(0xb800,(x * 2) + (y * 80 * 2));
	oscreen = (&introscn + 7) + ((x - 1) * 2) + (y * 80 * 2) + 1;
	while (*s)
	{
		*screen++ = *s++;
		if (attr != 0xff)
		{
			*screen++ = (attr & 0x8f) | (*oscreen & 0x70);
			oscreen += 2;
		}
		else
			screen++;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_ClearTextScreen() - Makes sure the screen is in text mode, clears it,
//              and moves the cursor to the leftmost column of the bottom line
//
///////////////////////////////////////////////////////////////////////////
static void
USL_ClearTextScreen(void)
{
	// Set to 80x25 color text mode
	_AL = 3;                                // Mode 3
	_AH = 0x00;
	geninterrupt(0x10);

	// Use BIOS to move the cursor to the bottom of the screen
	_AH = 0x0f;
	geninterrupt(0x10);             // Get current video mode into _BH
	_DL = 0;                                // Lefthand side of the screen
	_DH = 24;                               // Bottom row
	_AH = 0x02;
	geninterrupt(0x10);
}

///////////////////////////////////////////////////////////////////////////
//
//      US_TextScreen() - Puts up the startup text screen
//      Note: These are the only User Manager functions that can be safely called
//              before the User Mgr has been started up
//
///////////////////////////////////////////////////////////////////////////
void
US_TextScreen(void)
{
	word    i,n;

	USL_ClearTextScreen();

	_fmemcpy(MK_FP(0xb800,0),7 + &introscn,80 * 25 * 2);

	// Check for TED launching here
	for (i = 1;i < _argc;i++)
	{
		n = US_CheckParm(_argv[i],ParmStrings);
		if (n == 0)
		{
			tedlevelnum = atoi(_argv[i + 1]);
			tedlevel = true;
			return;
		}
		else if (n == 1)
		{
			NoWait = true;
			return;
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_Show() - Changes the appearance of one of the fields on the text
//              screen. Possibly adds a checkmark in front of it and highlights it
//
///////////////////////////////////////////////////////////////////////////
static void
USL_Show(word x,word y,word w,boolean show,boolean hilight)
{
	byte    far *screen,far *oscreen;

	screen = MK_FP(0xb800,((x - 1) * 2) + (y * 80 * 2));
	oscreen = (&introscn + 7) + ((x - 1) * 2) + (y * 80 * 2) - 1;
	*screen++ = show? 251 : ' ';    // Checkmark char or space
//      *screen = 0x48;
//      *screen = (*oscreen & 0xf0) | 8;
	oscreen += 2;
	if (show && hilight)
	{
		for (w++;w--;screen += 2,oscreen += 2)
			*screen = (*oscreen & 0xf0) | 0x0f;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_ShowMem() - Right justifies a longword in one of the memory fields on
//              the text screen
//
///////////////////////////////////////////////////////////////////////////
static void
USL_ShowMem(word x,word y,long mem)
{
	char    buf[16];
	word    i;

	for (i = strlen(ltoa(mem,buf,10));i < 5;i++)
		USL_ScreenDraw(x++,y," ",0xff);
	USL_ScreenDraw(x,y,buf,0xff);
}

///////////////////////////////////////////////////////////////////////////
//
//      US_UpdateTextScreen() - Called after the ID libraries are started up.
//              Displays what hardware is present.
//
///////////////////////////////////////////////////////////////////////////
void
US_UpdateTextScreen(void)
{
	boolean         b;
	longword        totalmem;

	// Show video card info
#if GRMODE == CGAGR
	USL_Show(21,7,4,true,true);
#elif GRMODE == EGAGR
	USL_Show(21,8,7,true,true);
	if (compatibility)
		USL_ScreenDraw(5,10,"SVGA Compatibility Mode Enabled.",0x4f);
#elif GRMODE == TGAGR
	USL_Show(21,9,5,true,true);
#endif

	// Show input device info
	USL_Show(60,7,8,true,true);
	USL_Show(60,8,11,JoysPresent[0],true);
	USL_Show(60,9,11,JoysPresent[1],true);
	USL_Show(60,10,5,MousePresent,true);

	// Show sound hardware info
	USL_Show(21,14,11,true,SoundMode == sdm_PC);
	b = (SoundMode == sdm_AdLib) || (MusicMode == smm_AdLib);
	USL_Show(21,15,14,AdLibPresent,b);
	if (b && AdLibPresent)  // Hack because of two lines
	{
		byte    far *screen,far *oscreen;
		word    x,y,w;

		x = 21;
		y = 16;
		w = 14;
		screen = MK_FP(0xb800,(x * 2) + (y * 80 * 2) - 1);
		oscreen = (&introscn + 7) + (x * 2) + (y * 80 * 2) - 1;
		oscreen += 2;
		for (w++;w--;screen += 2,oscreen += 2)
			*screen = (*oscreen & 0xf0) | 0x0f;
	}
	USL_Show(21,17,5,true,MusicMode == smm_Tandy);

	// Show memory available/used
	USL_ShowMem(63,15,mminfo.mainmem / 1024);
	USL_Show(53,15,23,true,true);
	USL_ShowMem(63,16,mminfo.EMSmem / 1024);
	USL_Show(53,16,23,mminfo.EMSmem? true : false,true);
	USL_ShowMem(63,17,mminfo.XMSmem / 1024);
	USL_Show(53,17,23,mminfo.XMSmem? true : false,true);
	totalmem = mminfo.mainmem + mminfo.EMSmem + mminfo.XMSmem;
	USL_ShowMem(63,18,totalmem / 1024);
	USL_Show(53,18,23,true,true);   // DEBUG
	USL_ScreenDraw(52,18," ",0xff);

	// Change Initializing... to Loading...
	USL_ScreenDraw(27,22,"  Loading...   ",0x9c);
}

///////////////////////////////////////////////////////////////////////////
//
//      US_FinishTextScreen() - After the main program has finished its initial
//              loading, this routine waits for a keypress and then clears the screen
//
///////////////////////////////////////////////////////////////////////////
void
US_FinishTextScreen(void)
{
static  byte    colors[] = {4,6,13,15,15,15,15,15,15};
		boolean up;
		int             i,c;

	// Change Loading... to Press a Key

	if (!(tedlevel || NoWait))
	{
		IN_ClearKeysDown();
		for (i = 0,up = true;!IN_UserInput(4,true);)
		{
			c = colors[i];
			if (up)
			{
				if (++i == 9)
					i = 8,up = false;
			}
			else
			{
				if (--i < 0)
					i = 1,up = true;
			}

			USL_ScreenDraw(29,22," Ready - Press a Key     ",0x00 + c);
		}
	}
	else
		USL_ScreenDraw(29,22," Ready - Press a Key     ",0x9a);

	IN_ClearKeysDown();

	USL_ClearTextScreen();
}

//      Window/Printing routines

///////////////////////////////////////////////////////////////////////////
//
//      USL_SetPrintRoutines() - Sets the routines used to measure and print
//              from within the User Mgr. Primarily provided to allow switching
//              between masked and non-masked fonts
//
///////////////////////////////////////////////////////////////////////////
void
USL_SetPrintRoutines(void (*measure)(char far *,word *,word *),void (*print)(char far *))
{
	USL_MeasureString = measure;
	USL_DrawString = print;
}

///////////////////////////////////////////////////////////////////////////
//
//      US_Print() - Prints a string in the current window. Newlines are
//              supported.
//
///////////////////////////////////////////////////////////////////////////
void
US_Print(char *s)
{
	char    c,*se;
	word    w,h;

	while (*s)
	{
		se = s;
		while ((c = *se) && (c != '\n'))
			se++;
		*se = '\0';

		USL_MeasureString(s,&w,&h);
		px = PrintX;
		py = PrintY;
		USL_DrawString(s);

		s = se;
		if (c)
		{
			*se = c;
			s++;

			PrintX = WindowX;
			PrintY += h;
		}
		else
			PrintX += w;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//      US_PrintUnsigned() - Prints an unsigned long
//
///////////////////////////////////////////////////////////////////////////
void
US_PrintUnsigned(longword n)
{
	char    buffer[32];

	US_Print(ultoa(n,buffer,10));
}

///////////////////////////////////////////////////////////////////////////
//
//      US_PrintSigned() - Prints a signed long
//
///////////////////////////////////////////////////////////////////////////
void
US_PrintSigned(long n)
{
	char    buffer[32];

	US_Print(ltoa(n,buffer,10));
}

///////////////////////////////////////////////////////////////////////////
//
//      US_PrintCentered() - Prints a string centered in the current window.
//
///////////////////////////////////////////////////////////////////////////
void
US_PrintCentered(char *s)
{
	word    w,h;

	USL_MeasureString(s,&w,&h);

	px = WindowX + ((WindowW - w) / 2);
	py = WindowY + ((WindowH - h) / 2);
	USL_DrawString(s);
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_CPrintLine() - Prints a string centered on the current line and
//              advances to the next line. Newlines are not supported.
//
///////////////////////////////////////////////////////////////////////////
void
USL_CPrintLine(char *s)
{
	word    w,h;

	USL_MeasureString(s,&w,&h);

	if (w > WindowW)
		Quit("USL_CPrintLine() - String exceeds width");
	px = WindowX + ((WindowW - w) / 2);
	py = PrintY;
	USL_DrawString(s);
	PrintY += h;
}

///////////////////////////////////////////////////////////////////////////
//
//      US_CPrint() - Prints a string in the current window. Newlines are
//              supported.
//
///////////////////////////////////////////////////////////////////////////
void
US_CPrint(char *s)
{
	char    c,*se;

	while (*s)
	{
		se = s;
		while ((c = *se) && (c != '\n'))
			se++;
		*se = '\0';

		USL_CPrintLine(s);

		s = se;
		if (c)
		{
			*se = c;
			s++;
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_ClearWindow() - Clears the current window to white and homes the
//              cursor
//
///////////////////////////////////////////////////////////////////////////
void
USL_ClearWindow(void)
{
	VWB_Bar(WindowX,WindowY,WindowW,WindowH,WHITE);
	PrintX = WindowX;
	PrintY = WindowY;
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_DrawWindow() - Draws a frame and sets the current window parms
//
///////////////////////////////////////////////////////////////////////////
void
USL_DrawWindow(word x,word y,word w,word h)
{
	word    i,
			sx,sy,sw,sh;

	WindowX = x * 8;
	WindowY = y * 8;
	WindowW = w * 8;
	WindowH = h * 8;

	PrintX = WindowX;
	PrintY = WindowY;

	sx = (x - 1) * 8;
	sy = (y - 1) * 8;
	sw = (w + 1) * 8;
	sh = (h + 1) * 8;

	USL_ClearWindow();

	VWB_DrawTile8M(sx,sy,0),VWB_DrawTile8M(sx,sy + sh,6);
	for (i = sx + 8;i <= sx + sw - 8;i += 8)
		VWB_DrawTile8M(i,sy,1),VWB_DrawTile8M(i,sy + sh,7);
	VWB_DrawTile8M(i,sy,2),VWB_DrawTile8M(i,sy + sh,8);

	for (i = sy + 8;i <= sy + sh - 8;i += 8)
		VWB_DrawTile8M(sx,i,3),VWB_DrawTile8M(sx + sw,i,5);
}

///////////////////////////////////////////////////////////////////////////
//
//      US_CenterWindow() - Generates a window of a given width & height in the
//              middle of the screen
//
///////////////////////////////////////////////////////////////////////////
void
US_CenterWindow(word w,word h)
{
	USL_DrawWindow(((MaxX / 8) - w) / 2,((MaxY / 8) - h) / 2,w,h);
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_SaveWindow() - Saves the current window parms into a record for
//              later restoration
//
///////////////////////////////////////////////////////////////////////////
void
USL_SaveWindow(WindowRec *win)
{
	win->x = WindowX;
	win->y = WindowY;
	win->w = WindowW;
	win->h = WindowH;

	win->px = PrintX;
	win->py = PrintY;
}

///////////////////////////////////////////////////////////////////////////
//
//      USL_RestoreWindow() - Sets the current window parms to those held in the
//              record
//
///////////////////////////////////////////////////////////////////////////
void
USL_RestoreWindow(WindowRec *win)
{
	WindowX = win->x;
	WindowY = win->y;
	WindowW = win->w;
	WindowH = win->h;

	PrintX = win->px;
	PrintY = win->py;
}

//      Input routines

///////////////////////////////////////////////////////////////////////////
//
//      USL_XORICursor() - XORs the I-bar text cursor. Used by US_LineInput()
//
///////////////////////////////////////////////////////////////////////////
static void
USL_XORICursor(int x,int y,char *s,word cursor)
{
	char    buf[MaxString];
	word    w,h;

	strcpy(buf,s);
	buf[cursor] = '\0';
	USL_MeasureString(buf,&w,&h);

	px = x + w - 1;
	py = y;
	USL_DrawString("\x80");
}

///////////////////////////////////////////////////////////////////////////
//
//      US_LineInput() - Gets a line of user input at (x,y), the string defaults
//              to whatever is pointed at by def. Input is restricted to maxchars
//              chars or maxwidth pixels wide. If the user hits escape (and escok is
//              true), nothing is copied into buf, and false is returned. If the
//              user hits return, the current string is copied into buf, and true is
//              returned
//
///////////////////////////////////////////////////////////////////////////
boolean
US_LineInput(int x,int y,char *buf,char *def,boolean escok,
				int maxchars,int maxwidth)
{
	boolean         redraw,
				cursorvis,cursormoved,
				done,result;
	ScanCode        sc;
	char            c,
				s[MaxString],olds[MaxString];
	word            i,
				cursor,
				w,h,
				len;
	longword        lasttime;

	if (def)
		strcpy(s,def);
	else
		*s = '\0';
	*olds = '\0';
	cursor = strlen(s);
	cursormoved = redraw = true;

	cursorvis = done = false;
	lasttime = TimeCount;
	LastASCII = key_None;
	LastScan = sc_None;

	while (!done)
	{
		if (cursorvis)
			USL_XORICursor(x,y,s,cursor);

	asm     pushf
	asm     cli

		sc = LastScan;
		LastScan = sc_None;
		c = LastASCII;
		LastASCII = key_None;

	asm     popf

		switch (sc)
		{
		case sc_LeftArrow:
			if (cursor)
				cursor--;
			c = key_None;
			cursormoved = true;
			break;
		case sc_RightArrow:
			if (s[cursor])
				cursor++;
			c = key_None;
			cursormoved = true;
			break;
		case sc_Home:
			cursor = 0;
			c = key_None;
			cursormoved = true;
			break;
		case sc_End:
			cursor = strlen(s);
			c = key_None;
			cursormoved = true;
			break;

		case sc_Return:
			strcpy(buf,s);
			done = true;
			result = true;
			c = key_None;
			break;
		case sc_Escape:
			if (escok)
			{
				done = true;
				result = false;
			}
			c = key_None;
			break;

		case sc_BackSpace:
			if (cursor)
			{
				strcpy(s + cursor - 1,s + cursor);
				cursor--;
				redraw = true;
			}
			c = key_None;
			cursormoved = true;
			break;
		case sc_Delete:
			if (s[cursor])
			{
				strcpy(s + cursor,s + cursor + 1);
				redraw = true;
			}
			c = key_None;
			cursormoved = true;
			break;

		case 0x4c:      // Keypad 5
		case sc_UpArrow:
		case sc_DownArrow:
		case sc_PgUp:
		case sc_PgDn:
		case sc_Insert:
			c = key_None;
			break;
		}

		if (c)
		{
			len = strlen(s);
			USL_MeasureString(s,&w,&h);

			if
			(
				isprint(c)
			&&      (len < MaxString - 1)
			&&      ((!maxchars) || (len < maxchars))
			&&      ((!maxwidth) || (w < maxwidth))
			)
			{
				for (i = len + 1;i > cursor;i--)
					s[i] = s[i - 1];
				s[cursor++] = c;
				redraw = true;
			}
		}

		if (redraw)
		{
			px = x;
			py = y;
			USL_DrawString(olds);
			strcpy(olds,s);

			px = x;
			py = y;
			USL_DrawString(s);

			redraw = false;
		}

		if (cursormoved)
		{
			cursorvis = false;
			lasttime = TimeCount - TickBase;

			cursormoved = false;
		}
		if (TimeCount - lasttime > TickBase / 2)
		{
			lasttime = TimeCount;

			cursorvis ^= true;
		}
		if (cursorvis)
			USL_XORICursor(x,y,s,cursor);

		VW_UpdateScreen();
	}

	if (cursorvis)
		USL_XORICursor(x,y,s,cursor);
	if (!result)
	{
		px = x;
		py = y;
		USL_DrawString(olds);
	}
	VW_UpdateScreen();

	IN_ClearKeysDown();
	return(result);
}
