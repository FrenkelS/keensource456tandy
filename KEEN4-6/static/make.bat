@echo off
rem bcc makeobj.c

rem makeobj c AUDIODCT.CK4 ..\keen4\ck4adict.obj _audiodict
makeobj f AUDIOHED.CK4 ..\keen4\ck4ahead.obj _AudioHeader _audiohead
makeobj c EGADICT.CK4  ..\keen4\ck4edict.obj _EGAdict
makeobj f EGAHEAD.CK4  ..\keen4\ck4ehead.obj EGA_grafixheader _EGAhead
makeobj c CGADICT.CK4  ..\keen4\ck4cdict.obj _CGAdict
makeobj f CGAHEAD.CK4  ..\keen4\ck4chead.obj CGA_grafixheader _CGAhead
makeobj f TGAHEAD.CK4  ..\keen4\ck4thead.obj TGA_grafixheader _TGAhead
makeobj f MAPHEAD.CK4  ..\keen4\ck4mhead.obj MapHeader _maphead
makeobj f introscn.CK4 ..\keen4\ck4intro.obj _introscn
