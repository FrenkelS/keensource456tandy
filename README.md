# Commander Keen 4 Tandy 320x200 16 color source code port
Download the game [here](https://github.com/FrenkelS/keensource456tandy/releases)

This is a Tandy 320x200 16 color version of Commander Keen 4.
It includes Tandy music.
There's also a version for MCGA with smooth palette fading.
And EGA and CGA versions that feature the same Tandy music.

If the graphics get corrupted in the Tandy version while playing one or two levels, like in [this video](https://youtu.be/zyuhOdDRiHk?t=6211),
you don't have enough video memory allocated. This version of the game needs 32k of video memory.
The amount of video memory can be changed in the BIOS or by using [ADJMEM](https://www.classicdosgames.com/tutorials/grafix/chapter4.html).
Before running `KEEN4T.EXE`, run `ADJMEM -16` to increase the amount of video memory by 16k.
There's always at least 16k of video memory allocated, so increasing it by 16k gives you the required amount of 32k of video memory.

The score box is disabled by default. The game runs faster this way.
Pressing Backspace enables the score box.

If you can create executables that are byte for byte 100% identical
to the Keen 4 v1.4 EGA and CGA executables with K1n9_Duk3's reconstructed Commander Keen 4-6 source code, you can probably also compile this code.

## Special Thanks
* [id Software](https://idsoftware.com) for creating Commander Keen.

* [K1n9_Duk3](https://k1n9duk3.shikadi.net) for reconstructing the original Keen 4-6 source code.

* [Einar Saukas](https://github.com/einar-saukas) and [Emmanuel Marty](https://github.com/emmanuel-marty) for [ZX0](https://github.com/emmanuel-marty/unzx0_x86).
I had to replace the original Huffman compression with something that produces smaller files, otherwise the game wouldn't fit on a 720 kB floppy disk.

* [ModdingWiki](https://moddingwiki.shikadi.net) for information about the Commander Keen file formats.