# Commander Keen 4 Tandy 320x200 16 color source code port
This is a Tandy 320x200 16 color version of Commander Keen 4.
You need to provide your own copy of GAMEMAPS.CK4 to make it run.
You need to provide your own copy of INTROSCN.CK4 and MAPHEAD.CK4 to make it compile.

Speaking of compiling, if you can create executables that are byte for byte 100% identical
to the Keen 4 v1.4 EGA and CGA executables with K1n9_Duk3's reconstructed Commander Keen 4-6 source code, you can probably also compile this code.

## Special Thanks
I would like to thank id Software for creating Commander Keen.

Special thanks to K1n9_Duk3 for reconstructing the original Keen 4-6 source code.

And special thanks to Emmanuel Marty for [LZSA2](https://github.com/emmanuel-marty/lzsa).
I had to replace the original Huffman compression with something that produces smaller files, otherwise the game wouldn't fit on a 720 KB floppy disk.