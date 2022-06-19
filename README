## Tiled Map Converter
# What is it?
The Tiled Map Converter is a program to convert some [Tiled](https://github.com/mapeditor/tiled) save files into some other file types. Specifically, it accepts Tiled saves using CSV, with three layers named Tilemap, Collision, and Priority, with a map size no greater than 32 by 32 tiles. The output type is selected at compile time, between a few Gameboy-focused file formats.
# Why is it?
I wrote this program because I wanted to use an existing program for making maps for my Gameboy game, but I needed the maps to be smaller-sized than a Tiled save, and the export options helped little. By writing this program, I got to choose a map format that was optimized for size, only using features relevant to the Gameboy.
# How do I use it?
`MapConv.exe infile.tmx outfile.gbm`
The output format is determined at compile time. A Code::Blocks project file has been provided to select between -three- two output options. There are restrictions on the type of saves that may be used; refer above on such restrictions.
# Why is it a mess? Why does it suck?
I made this program before I was competent in basic project management. I still don't have a solid grasp on compile options with an IDE.
# How do I improve it?
Mimic one of the *Write.c files in your own target. Building will only care that
 1. There is only 1 *Write.c file included in the target.
 2. The one function's signature is correct and respected.

Finally, see the [Marisa game](https://github.com/ToriningenGames/Marisa-GB-1) for some examples of working, correct Tiled files.