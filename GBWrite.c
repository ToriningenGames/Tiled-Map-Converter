#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "formats.h"



struct tilePart {
	int16_t tileNo;
	bool isTile;	//Indicates that we actually change a tile here, and that tileNo is valid
	bool isEnd;
	bool editColl;
	bool editVisi;
	bool editPrio;
	bool valColl;
	bool valVisi;
	bool valPrio;
	struct tilePart *maxbackref;
	uint16_t matLen;
};


uint16_t findLen(struct tilePart *a, struct tilePart *b)
{
	uint16_t length = 0;
	while (!a->isEnd && !b->isEnd
	&& a->isTile == b->isTile
	&& (a->isTile? (a->tileNo == b->tileNo) : 1)
	&& a->editColl == b->editColl
	&& (a->editColl? (a->valColl == b->valColl) : 1)
	&& a->editPrio == b->editPrio
	&& (a->editPrio? (a->valPrio == b->valPrio) : 1)
	&& a->editVisi == b->editVisi
	&& (a->editVisi? (a->valVisi == b->valVisi) : 1)) {
		a++;
		b++;
		length++;
	}
	return length;
}

//Given a tile, put raw data into output stream
//Returns next tile to interpret
struct tilePart *genTile(struct tilePart *tile, uint8_t *dest, uint16_t *dstOff, int remaining)
{
	static int prevAttEdit = -1;
	static int base = -1;
	if (tile->isEnd)
		return tile;
	if (tile->isTile) {
		if (base == -1 || tile->tileNo < base || tile->tileNo > base + 15) {
			//New base
			//Choosing a base:
				//Pick base that lasts the furthest
				//Ignore tiles that have matLen > 2
				//Bases must be multiple of 4!
			uint8_t minBase = tile->tileNo < 15 ? 0 : (tile->tileNo - 16 + 4) & 0xFC;
			uint8_t maxBase = tile->tileNo & 0xFC;
			//maxBase moves as lo as needed to service the next tile
			//minBase moves as hi as needed to service the next tile
			//When minBase > maxBase, we have a maximum range in the previous tile
			struct tilePart *basePeek = tile;
			while (minBase < maxBase && --remaining) {
				basePeek++;
				if (basePeek->isEnd) {
					base = minBase;
					break;
				};
				if (!basePeek->isTile)
					continue;
				if (basePeek->matLen > 2)
					continue;
				if (basePeek->tileNo < maxBase) {
					while (basePeek->tileNo < maxBase) {
						//Lower maxBase
						maxBase -= 4;
						base = minBase;
					}
				} else {
					while (basePeek->tileNo > minBase) {
						//Raise minBase
						minBase += 4;
						base = maxBase;
					}
				}
			}
			//Set the new base
			dest[*dstOff] = (uint8_t)base;
			(*dstOff)++;
		};
		//Insert tile
		dest[*dstOff] = (tile->tileNo - base) << 4 | (bool)tile->valPrio << 3 | (bool)tile->valColl << 2 | (bool)tile->valVisi << 1 | 0x01;
		(*dstOff)++;
		tile++;
	} else {
		//Not tile
		if (tile->editColl || tile->editPrio || tile->editVisi) {
			//Attribute tile
			if (prevAttEdit == -1 || prevAttEdit != tile->editColl * 4 + tile->editPrio * 2 + tile->editVisi) {
				//Set what changes
				prevAttEdit = tile->editColl * 4 + tile->editPrio * 2 + tile->editVisi;
				dest[*dstOff] = 0x0E | (bool)(tile->valVisi)<<5 | (bool)(tile->valColl)<<6 | (bool)(tile->valPrio)<<7;
				(*dstOff)++;
			};
			//Set the changes
			dest[*dstOff] = 0x1E | (bool)(tile->valVisi)<<5 | (bool)(tile->valColl)<<6 | (bool)(tile->valPrio)<<7;
			(*dstOff)++;
			//Next tile
			tile++;
		} else {
			//Skip tile completely
			//Count no. of tiles to skip completely
			uint16_t skipCnt = 1;
			while (!(tile+skipCnt)->isEnd && !((tile+skipCnt)->isTile || (tile+skipCnt)->editColl || (tile+skipCnt)->editPrio || (tile+skipCnt)->editVisi))
				skipCnt++;
			dest[*dstOff++] = 0x06;	//Offset of 0
			dest[*dstOff++] = 0x00;
			dest[*dstOff++] = (uint8_t)skipCnt;	//Length
			tile += skipCnt;	//Those tiles are accounted for
		}
	}
	return tile;
}

//Given a tile, put LZ data into output stream
struct tilePart *genLZ(struct tilePart *tile, uint8_t *dest, uint16_t *dstOff, int remaining)
{
	//If the tile repeat is too long, just stick to 255.
	//The next tile will contain a comparable backreference if applicable
	uint16_t offset = tile - tile->maxbackref;
	if (offset > 0x03FF)	//We can't handle offsets larger than 10 bits
		return genTile(tile, dest, dstOff, remaining);	//Die gracefully
	uint8_t length = tile->matLen > 255 ? 255 : tile->matLen;
	//Insert LZ declaration
	dest[*dstOff] = 0x06 | ((offset & 0x0300) >> 2);
	(*dstOff)++;
	dest[*dstOff] = offset & 0xFF;
	(*dstOff)++;
	dest[*dstOff] = length;
	(*dstOff)++;
	tile += length;
	return tile;
}

#define MAKENOT(X, Y, Z) ((X) = ((X)==(Y))?(Z):(Y))

uint8_t *outputParse(struct map *source, uint16_t *outsize)
{
	*outsize = 0;
	int mapsize = source->width * source->height;
	uint8_t *output = malloc(mapsize * 2);
	//Copy to tilePart array
	struct tilePart *tilearr = malloc(mapsize * sizeof(struct tilePart));
	for (uint16_t i = 0; i < mapsize; i++) {
		//For every tile
		tilearr[i].isEnd = false;
		tilearr[i].isTile = source->tiledata[i] != -1;
		tilearr[i].tileNo = source->tiledata[i];
		tilearr[i].editColl = source->attrdata[i].docollision;
		tilearr[i].editVisi = source->attrdata[i].dovisible;
		tilearr[i].editPrio = source->attrdata[i].dopriority;
		tilearr[i].valColl = source->attrdata[i].collision;
		tilearr[i].valVisi = source->attrdata[i].visible;
		tilearr[i].valPrio = source->attrdata[i].priority;
		tilearr[i].matLen = 0;
		tilearr[i].maxbackref = NULL;
	}
	//Terminus does not match any tile
	tilearr[mapsize].isTile = false;
	tilearr[mapsize].isEnd = true;
	//Get the maximum LZ backreference for each tile
	//We had to fill the array first because backreferences can go past the current tile
	for (uint16_t i = 0; i < mapsize; i++) {
		for (uint16_t j = 0; j < i; j++) {
			if (tilearr[i].matLen <= findLen(tilearr+i, tilearr+j)) {	//<= gets most recent maximum match
				tilearr[i].maxbackref = tilearr+j;
				tilearr[i].matLen = findLen(tilearr+i, tilearr+j);
			};
		}
	}
	//Every tile now holds its maximum backreference.
		//If backref >= 3, insert
		//If backref <= 1, do not
		//If backref == 2, and this byte or the next skips setting tile, insert
		//Else, do not (inefficient if base is changed, but that info cannot be decided)
	for (struct tilePart *i = tilearr; !i->isEnd; ) {
		if (!i->isTile) {
			//Entry does not set tile
			if (i->matLen >= 2) {
				//LZ it
				i = genLZ(i, output, outsize, mapsize - (i - tilearr));
			} else {
				//Output raw
				i = genTile(i, output, outsize, mapsize - (i - tilearr));
			}
		} else {
			//Entry does set tile
			if (i->matLen > 2) {
				//LZ it
				i = genLZ(i, output, outsize, mapsize - (i - tilearr));
			} else if (i->matLen == 2 && !(i+1)->isTile) {
				//Still LZ it
				i = genLZ(i, output, outsize, mapsize - (i - tilearr));
			} else {
				//Output raw
				i = genTile(i, output, outsize, mapsize - (i - tilearr));
			}
		}
	}
	output[*outsize] = 0x16;	//Ending byte
	(*outsize)++;
	return output;
}
