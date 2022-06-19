#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "yxml.h"
#include "formats.h"

#define BUFFER 65536


struct map *inputParse(FILE *infile)
{
	//Read the entire document into a string
	unsigned long size = 0;
	for (; getc(infile) != EOF; size++)
		;
	rewind(infile);
	char *input = malloc(size+1);
	input[size] = 0;
	fread(input, 1, size, infile);
	fclose(infile);
	//Start up yxml
	yxml_t *xmlBuf = malloc(sizeof(yxml_t) + BUFFER);
	yxml_init(xmlBuf, xmlBuf+1, BUFFER);
	//Prep output
	bool inMap = false;
	bool inLayer = false;
	int systemStart = 0;	//Tiled 1.3.4 changed the way tile numbers were assigned
	int tilesStart = 1;	//and I'm sour about it because now I have to open this project again.
	int startBuffer = 0;	//I hate XML.
	enum dest {dest_none, dest_tile, dest_coll, dest_visi, dest_prio};
	enum dest destination = dest_none;
	struct map *output = malloc(sizeof(struct map));
	char attrVal[32];
	attrVal[0] = '\0';
	output->height = -1;
	output->width = -1;
	int mapRef = 0;
	//Parse the document
	for (; *input; input++) {
		yxml_ret_t status = yxml_parse(xmlBuf, *input);
		if (status < 0) {
			//Error
			printf("Error parsing XML input at %u:%u\n", xmlBuf->line, xmlBuf->byte);
			return NULL;
		};
		switch (status) {
		case YXML_ELEMSTART :
			if (!strcmp(xmlBuf->elem, "map")) {
				inMap = true;
			} else {
				inMap = false;
				if (!strcmp(xmlBuf->elem, "layer"))
					inLayer = true;
			}
			break;
		case YXML_ELEMEND :
			if (destination == dest_tile) {
				output->tiledata[mapRef]-=tilesStart;	//Get the last tile too
			};
			//If we were collecting data, we're not anymore
			destination = dest_none;
			break;
		case YXML_ATTREND :
			//Check for interesting attributes
			//Attribute data is guaranteed to be single-byte (as far as we care)
			if (!strcmp(xmlBuf->attr, "firstgid")) {
				//following attribute defines starting tile number for... some tile bank.
				startBuffer = atoi(attrVal);
			};
			if (!strcmp(xmlBuf->attr, "source")) {
				//following attribute defines where to put the found starting tile number.
				if (!strcmp(attrVal, "System.tsx")) {
					systemStart = startBuffer;
				} else {
					tilesStart = startBuffer;
				}
			};
			if (inMap && !strcmp(xmlBuf->attr, "width")) {
				//following attribute defines map width
				output->width = atoi(attrVal);
			} else if (inMap && !strcmp(xmlBuf->attr, "height")) {
				//following attribute defines map height
				output->height = atoi(attrVal);
			};
			if (inLayer && !strcmp(xmlBuf->attr, "name")) {
				//following attribute names this map, and determines where the data goes
				if (!strcmp(attrVal, "Tilemap")) {
					destination = dest_tile;
				} else if (!strcmp(attrVal, "Collision")) {
					destination = dest_coll;
				} else if (!strcmp(attrVal, "Visibility")) {
					destination = dest_visi;
				} else if (!strcmp(attrVal, "Priority")) {
					destination = dest_prio;
				};
				mapRef = 0;
			};
			if (inMap && output->height >= 0 && output->width >= 0) {
				output->tiledata = calloc(output->height * output->width, sizeof(*output->tiledata));
				output->attrdata = calloc(output->height * output->width, sizeof(*output->attrdata));
			};
			//Reset the buffer
			attrVal[0] = '\0';
			break;
		case YXML_ATTRVAL :
			//Append data to active value buffer, if any
			if (strlen(attrVal) + strlen(xmlBuf->data) < 31)
				strcat(attrVal, xmlBuf->data);
			break;
		case YXML_CONTENT :
			//We're assuming all content is CSV tile data that conforms to the header
			//If we're collecting data, these CSV text fields get fed to a destination
			if (!destination)
				break;
			if (isspace(xmlBuf->data[0]))
				break;
			if (xmlBuf->data[0] == ',') {
				if (destination == dest_tile) {
					if (output->tiledata[mapRef] <= 0) {
						//Missing tile
						output->tiledata[mapRef] = -1;
					} else {
						output->tiledata[mapRef] -= tilesStart;	//Tiles are offset from zero. Some versions of Tiled count from 1.
						output->tiledata[mapRef] &= 0xFF;
					}
				}
				mapRef++;
				break;
			};
			if (destination == dest_tile) {
				if (xmlBuf->data[0] == '-') {
					//Negative value, likely missing tile
					output->tiledata[mapRef] = -1;
				} else {
					output->tiledata[mapRef] *= 10;
					output->tiledata[mapRef] += atoi(xmlBuf->data);
				}
			//Properties are affected if any tile is there, and they are set on even (left) tiles, and clear on odd (right) tiles
			} else if (destination == dest_coll) {
				output->attrdata[mapRef].docollision |= (bool)(atoi(xmlBuf->data)+1);
				output->attrdata[mapRef].collision = (atoi(xmlBuf->data)-systemStart) % 2;
			} else if (destination == dest_visi) {
				output->attrdata[mapRef].dovisible |= (bool)(atoi(xmlBuf->data)+1);
				output->attrdata[mapRef].visible = (atoi(xmlBuf->data)-systemStart) % 2;
			} else if (destination == dest_prio) {
				output->attrdata[mapRef].dopriority |= (bool)(atoi(xmlBuf->data)+1);
				output->attrdata[mapRef].priority = (atoi(xmlBuf->data)-systemStart) % 2;
			}
			break;
		case YXML_OK :
		default :
			//Something we don't care about
			break;
		}
	}
	
	//Close down yxml
	yxml_eof(xmlBuf);
	free(xmlBuf);
	
	return output;
}
