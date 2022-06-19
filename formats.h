#ifndef FORMATS_H_INCLUDED
#define FORMATS_H_INCLUDED

#include <stdio.h>
#include <stdint.h>

struct properties {
	char dopriority  :1;
	char docollision :1;
	char dovisible   :1;
	char priority    :1;
	char collision   :1;
	char visible     :1;
};

struct map {
	int width;
	int height;
	int16_t *tiledata;
	struct properties *attrdata;
};

extern struct map *inputParse(FILE *infile);
extern uint8_t *outputParse(struct map *source, uint16_t *outsize);

#endif // FORMATS_H_INCLUDED
