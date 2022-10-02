# Commander Keen 4 Tandy 320x200 16 color source code port
This is a Tandy 320x200 16 color version of Commander Keen 4.

If the graphics are corrupted, you probably need to run `ADJMEM -16` before running this game.
It needs 32k of video memory.

Pressing Backspace disables the score box. This speeds up the game.

If you can create executables that are byte for byte 100% identical
to the Keen 4 v1.4 EGA and CGA executables with K1n9_Duk3's reconstructed Commander Keen 4-6 source code, you can probably also compile this code.

## Special Thanks
* [id Software](https://idsoftware.com) for creating Commander Keen.

* [K1n9_Duk3](https://k1n9duk3.shikadi.net) for reconstructing the original Keen 4-6 source code.

* [Einar Saukas](https://github.com/einar-saukas) and [Emmanuel Marty](https://github.com/emmanuel-marty) for [ZX0](https://github.com/emmanuel-marty/unzx0_x86).
I had to replace the original Huffman compression with something that produces smaller files, otherwise the game wouldn't fit on a 720 KB floppy disk.

* [ModdingWiki](https://moddingwiki.shikadi.net) for information about the Commander Keen file formats.