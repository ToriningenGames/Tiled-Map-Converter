#include <stdlib.h>
#include <string.h>
#include "formats.h"

//Outputs mapdata into an uncompressed flat stream

uint8_t *outputParse(struct map *source, uint16_t *outsize)
{
	*outsize = 32 * 32 + 32 * 32 * 3 / 8;
	uint8_t *outdata = malloc(*outsize);
	//Step 1: Resize the map to 32 x 32, and fill the extra space with blanks
	memset(outdata,           0x30,    32 * 32);
	memset(outdata + 32 * 32,   0, 3 * 32 * 32 / 8);
	for (int y = 0; y < source->height; y++) {
		for (int x = 0; x < source->width; x++) {
			outdata[y * 32 + x] = source->tiledata[y * source->width + x];
		}
	}
	//Step 2: Convert the attribute data to a reasonable byte format
	//Priority
	for (int y = 0; y < source->height; y++) {
		for (int x = 0; x < source->width; x++) {
			outdata[32*32 + (y * 32 + x) / 8] |= ((_Bool)source->attrdata[y * source->width + x].priority) << (7 - x % 8);
		}
	}
	//Collision
	for (int y = 0; y < source->height; y++) {
		for (int x = 0; x < source->width; x++) {
			outdata[32*32+128 + (y * 32 + x) / 8] |= ((_Bool)source->attrdata[y * source->width + x].collision) << (7 - x % 8);
		}
	}
	//Visibility
	for (int y = 0; y < source->height; y++) {
		for (int x = 0; x < source->width; x++) {
			outdata[32*32+256 + (y * 32 + x) / 8] |= ((_Bool)source->attrdata[y * source->width + x].visible) << (7 - x % 8);
		}
	}
	return outdata;
}
