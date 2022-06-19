#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "formats.h"


void deleteMap(struct map *victim);

int main(int argc, char **argv)
{
	if (argc == 1) {
		puts("Usage: MapConv infile.tmx outfile.gbm");
		return 1;
	};
	FILE *infile = fopen(argv[1], "r");
	if (!infile) {
		perror("Can't open input");
		return 2;
	};
	struct map *interim = inputParse(infile);
	if (!interim)
		return 4;
	uint16_t outsize;
	uint8_t *outdata = outputParse(interim, &outsize);
	deleteMap(interim);
	if (outsize == 0xFFFF) {
		puts("Error creating Gameboy map: output too large.");
		return 3;
	};
	FILE *outfile = fopen(argv[2], "wb");
	fwrite(outdata, 1, outsize, outfile);
	fclose(outfile);
	free(outdata);
	return 0;
}

void deleteMap(struct map *victim)
{
	free(victim);
}
