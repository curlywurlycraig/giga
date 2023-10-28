#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define FILE_SIZE_BYTES 1024
#define NUM_LINES 250000000
#define LINE_LENGTH_BYTES 80
#define OUTPUT_FILE "dump_out.txt"

static char* line_template = "this is the line with the number %d\n";

int
main() 
{
    FILE *f = fopen(OUTPUT_FILE, "w");
    if (!f) {
	perror("failed to open file");
	return 1;
    }

    char strbuf[1024];
    for (int i = 0; i < NUM_LINES; i++) {
	sprintf(strbuf, line_template, i+1);
	fputs(strbuf, f);
    }

    fclose(f);

    return 0;
}
