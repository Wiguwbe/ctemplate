#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "generator.h"

int main(int argc, char**argv)
{
	char basename[128];
	int status;
	if(argc < 2) {
		fprintf(stderr, "usage: %s: <template-file>...\n", *argv);
		return 1;
	}

	/* TODO special configuration through envvars ? */

	for(int i=1; i<argc; i++) {
		// handle filename
		char *r = basename, *last_point=NULL;
		for(char *l=argv[i];*l;l++,r++) {
			*r = *l=='/'?*l:'_';
			if(*l == '.') last_point = r;
		}
		*r = 0;
		if(last_point) *last_point = 0;
		// open file
		FILE *file = fopen(argv[i], "r");
		if(!file) {
			PERROR(fopen);
			return 1;
		}
		// generate template
		status = generate_template(file, basename);

		if(fclose(file) == EOF) {
			PERROR(fclose);
			return 1;
		}

		if(status)
			return 1;
	}

	// else, all good
	return 0;
}
