#include <stdlib.h>
#include <string.h>
#include "formats.h"

//Outputs mapdata into an LZ compressed formatted stream

uint8_t *outputParse(struct map *source, uint16_t *outsize)
{
	//Step 1: Resize the map to 32 x 32, and fill the extra space with skips
		//...but save the old width/height for the header
	uint8_t width = source->width;
	uint8_t height = source->height;
	struct map fullmap = {
		.width = 32,
		.height = 32,
		.tiledata = malloc(sizeof(*fullmap.tiledata) * 32 * 32),
		.attrdata = malloc(sizeof(*fullmap.attrdata) * 32 * 32)
	};
	memset(fullmap.tiledata, -1, sizeof(*fullmap.tiledata) * 32 * 32);
	memset(fullmap.attrdata,  0, sizeof(*fullmap.attrdata) * 32 * 32);
	for (int y = 0; y < source->height; y++) {
		for (int x = 0; x < source->width; x++) {
			fullmap.tiledata[y * 32 + x] = source->tiledata[y * source->width + x];
		}
	}
	for (int y = 0; y < source->height; y++) {
		for (int x = 0; x < source->width; x++) {
			fullmap.attrdata[y * 32 + x] = source->attrdata[y * source->width + x];
		}
	}
	
	uint8_t *outdata = malloc(1024 + 128 * 3 + 2);
	uint8_t *outpoint = outdata;
	*outpoint++ = width * 8;	//tiles to pixels
	*outpoint++ = height * 8;
	//Step 2: LZ compress the data, except every -1 is always a miss
		//Every run of -1s gets its own LZ entry of backreference distance 0
	{
		int litlen = 0, litpoint = 0;
		for (int i = 0; i < 1024; i++) {
			//Is this a string of -1s?
			if (fullmap.tiledata[i] == -1) {
				//Literal encode first
				if (litlen) {
					*outpoint++ = (uint8_t)litlen | 0x80;
					while (litlen--)
						*outpoint++ = (uint8_t)fullmap.tiledata[litpoint++];
					litlen = 0;
				};
				//LZ encode the -1s
				//Find -1 string length
				int skiplen = 0;
				for (int j = i; j < 1024; j++) {
					if (fullmap.tiledata[j] == -1) {
						skiplen++;
					} else {
						break;
					}
					if (skiplen == 255)
						break;
				}
				*outpoint++ = 0;	//Backreference 0
				*outpoint++ = 0;
				*outpoint++ = skiplen;
				i += skiplen - 1;
				litpoint += skiplen;
				continue;
			}
			//Can we LZ compress here?
			int lzlen = 0, lzpoint = 0;
			for (int j = 0; j < i; j++) {
				int matchlen = 0;
				int16_t *str = fullmap.tiledata + j, *substr = fullmap.tiledata + i;
				while (*str == *substr && *str != -1) {
					str++;
					substr++;
					matchlen++;
					if (matchlen == 1024 - i)
						break;
				}
				if (matchlen > lzlen) {
					lzlen = (matchlen > 255)? 255 : matchlen;
					lzpoint = j;
				};
			}
			if (lzlen > 3) {
				//We can compress here
				//Literal first
				if (litlen) {
					*outpoint++ = (uint8_t)litlen | 0x80;
					while (litlen--)
						*outpoint++ = (uint8_t)fullmap.tiledata[litpoint++];
					litlen = 0;
				}
				//LZ entry
				*outpoint++ = (uint8_t)(((i - lzpoint) & 0xFF00) >> 8);
				*outpoint++ = (uint8_t)((i - lzpoint) & 0xFF);
				*outpoint++ = (uint8_t)lzlen;
				litpoint += lzlen;
				i += lzlen - 1;
			} else {
				//Another entry for the literal
				if (litlen == 0x7F) {
					//We can't make a longer literal; we must output and reset
					*outpoint++ = (uint8_t)litlen | 0x80;
					while (litlen--)
						*outpoint++ = (uint8_t)fullmap.tiledata[litpoint++];
					litlen = 0;
				};
				litlen++;
			}
		}
		//One last literal run
		if (litlen) {
			*outpoint++ = (uint8_t)litlen | 0x80;
			while (litlen--)
				*outpoint++ = (uint8_t)fullmap.tiledata[litpoint++];
			litlen = 0;
		}
	}
	//Step 3: Convert the attribute data to a reasonable byte format
	uint8_t *outattr = malloc(384);
	for (int i = 0; i < 1024; i+=8) {
		outattr[i/8] = ((fullmap.attrdata[i].dopriority & fullmap.attrdata[i].priority) & 1) << 7
			| ((fullmap.attrdata[i+1].dopriority & fullmap.attrdata[i+1].priority) & 1) << 6
			| ((fullmap.attrdata[i+2].dopriority & fullmap.attrdata[i+2].priority) & 1) << 5
			| ((fullmap.attrdata[i+3].dopriority & fullmap.attrdata[i+3].priority) & 1) << 4
			| ((fullmap.attrdata[i+4].dopriority & fullmap.attrdata[i+4].priority) & 1) << 3
			| ((fullmap.attrdata[i+5].dopriority & fullmap.attrdata[i+5].priority) & 1) << 2
			| ((fullmap.attrdata[i+6].dopriority & fullmap.attrdata[i+6].priority) & 1) << 1
			| ((fullmap.attrdata[i+7].dopriority & fullmap.attrdata[i+7].priority) & 1);
	}
	for (int i = 0; i < 1024; i+=8) {
		outattr[i/8+128] = ((fullmap.attrdata[i].docollision & fullmap.attrdata[i].collision) & 1) << 7
			| ((fullmap.attrdata[i+1].docollision & fullmap.attrdata[i+1].collision) & 1) << 6
			| ((fullmap.attrdata[i+2].docollision & fullmap.attrdata[i+2].collision) & 1) << 5
			| ((fullmap.attrdata[i+3].docollision & fullmap.attrdata[i+3].collision) & 1) << 4
			| ((fullmap.attrdata[i+4].docollision & fullmap.attrdata[i+4].collision) & 1) << 3
			| ((fullmap.attrdata[i+5].docollision & fullmap.attrdata[i+5].collision) & 1) << 2
			| ((fullmap.attrdata[i+6].docollision & fullmap.attrdata[i+6].collision) & 1) << 1
			| ((fullmap.attrdata[i+7].docollision & fullmap.attrdata[i+7].collision) & 1);
	}
	//for (int i = 0; i < 1024; i+=8) {
	//	outattr[i/8+256] = ((fullmap.attrdata[i].dovisible & fullmap.attrdata[i].visible) & 1) << 7
	//		| ((fullmap.attrdata[i+1].dovisible & fullmap.attrdata[i+1].visible) & 1) << 6
	//		| ((fullmap.attrdata[i+2].dovisible & fullmap.attrdata[i+2].visible) & 1) << 5
	//		| ((fullmap.attrdata[i+3].dovisible & fullmap.attrdata[i+3].visible) & 1) << 4
	//		| ((fullmap.attrdata[i+4].dovisible & fullmap.attrdata[i+4].visible) & 1) << 3
	//		| ((fullmap.attrdata[i+5].dovisible & fullmap.attrdata[i+5].visible) & 1) << 2
	//		| ((fullmap.attrdata[i+6].dovisible & fullmap.attrdata[i+6].visible) & 1) << 1
	//		| ((fullmap.attrdata[i+7].dovisible & fullmap.attrdata[i+7].visible) & 1);
	//}
	//Step 4: LZ compress the attribute data behind it
	{
		int litlen = 0, litpoint = 0;
		for (int i = 0; i < 256; i++) {
			//Can we LZ compress here?
			int lzlen = 0, lzpoint = 0;
			for (int j = 0; j < i; j++) {
				int matchlen = 0;
				uint8_t *str = outattr + j, *substr = outattr + i;
				while (*str++ == *substr++) {
					matchlen++;
					if (matchlen == 256 - i)
						break;
				}
				if (matchlen > lzlen) {
					lzlen = (matchlen > 255)? 255 : matchlen;
					lzpoint = j;
				};
			}
			if (lzlen > 3) {
				//We can compress here
				//Literal first
				if (litlen) {
					*outpoint++ = (uint8_t)litlen | 0x80;
					memcpy(outpoint, outattr + litpoint, litlen);
					outpoint += litlen;
					litpoint += litlen;
					litlen = 0;
				}
				//LZ entry
				*outpoint++ = (uint8_t)(((i - lzpoint) & 0xFF00) >> 8);
				*outpoint++ = (uint8_t)((i - lzpoint) & 0xFF);
				*outpoint++ = (uint8_t)lzlen;
				litpoint += lzlen;
				i += lzlen - 1;
			} else {
				//Another entry for the literal
				if (litlen == 0x7F) {
					//We can't make a longer literal; we must output and reset
					*outpoint++ = (uint8_t)litlen | 0x80;
					memcpy(outpoint, outattr + litpoint, litlen);
					outpoint += litlen;
					litpoint += litlen;
					litlen = 0;
				};
				litlen++;
			}
		}
		//One last literal run
		if (litlen) {
			*outpoint++ = (uint8_t)litlen | 0x80;
			memcpy(outpoint, outattr + litpoint, litlen);
		}
		*outsize = litlen + outpoint - outdata;
	}
	return outdata;
}
