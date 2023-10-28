#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>

#include "edit.h"

/*
  TODO Turn on raw mode, and load the next page when user presses J (and quit when user presses q)
  TODO Add a page data structure
  TODO Add character by character scrolling (with a cursor)
  TODO When scrolling to the end of the first bytes, load the next page
  TODO Make it possible to edit the page
  TODO Save when quitting
  TODO Accept an argument for which file to edit
*/

#define FILENAME "dump_out.txt"
#define BUF_BYTES 1024
#define OFFSET 2048

struct termios termios_before;

void
reset_termios()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_before);
}

void
enable_raw()
{
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &new_termios);

    termios_before = new_termios;
    atexit(reset_termios);

    new_termios.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
}

int
main()
{
    FILE *f = fopen(FILENAME, "rb+");
    fseek(f, OFFSET, SEEK_CUR);
    char buf[BUF_BYTES+1];
    buf[BUF_BYTES] = '\0';

    size_t bytes_read = fread(buf, sizeof(char), BUF_BYTES, f);
    printf("read: %zu\n", bytes_read);
    printf("%s\n", buf);

    enable_raw();

    char c;
    while (1) {
	c = getchar();
	switch (c) {
	case EOF:
	    goto end;
	case 'q':
	    goto end;
	default:
	    printf("%d", c);
	}
    }

end:

    fclose(f);

    return 0;
}
