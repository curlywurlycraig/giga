#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

#include "edit.h"

/*
  TODO Add character by character scrolling (with a cursor)
  TODO When scrolling to the end of the first bytes, load the next page
  TODO Make it possible to edit the page
  TODO Save when quitting
  TODO Accept an argument for which file to edit
  TODO Error handling
  TODO Don't hard code term width and height
  TODO Handle wrapping (e.g. what about very long documents with no line breaks?)
  TODO There is probably a nicer way of handling how buffers and pages interact
  This can be done by making an abstraction that allows to read buffer regions transparently
*/

#define FILENAME "dump_out.txt"
#define PAGE_SIZE_BYTES 10000
#define TERM_WIDTH 80
#define TERM_HEIGHT 40

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

    new_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    new_termios.c_iflag &= ~(ICRNL | IXON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
}

typedef struct {
    size_t offset;
    char buffer[PAGE_SIZE_BYTES];
} Page;


Page*
page_read(FILE *f, size_t offset)
{
    Page* result = malloc(sizeof(Page));
    result->offset = offset;
    fseek(f, offset, SEEK_SET);
    size_t bytes_read = fread(result->buffer, sizeof(char), PAGE_SIZE_BYTES-1, f);
    result->buffer[bytes_read] = 0;
    return result;
}

void
page_free(Page* p)
{
    free(p);
}


typedef struct {
    int cursor_x;
    int cursor_y;
    long cursor_text_index; // this is the index in the entire file bytes
    long view_offset;
    FILE *f;
    Page *curr_page;
} Buffer;


Buffer*
buffer_load(char *filename)
{
    Buffer *b = malloc(sizeof(Buffer));
    FILE *f = fopen(FILENAME, "rb+");
    Page *curr_page = page_read(f, 0);
    b->curr_page = curr_page;
    b->f = f;
    b->cursor_x = 0;
    b->cursor_y = 0;
    b->cursor_text_index = 0;
    b->view_offset = 0;
    return b;
}

void
buffer_draw(Buffer *b)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    // print TERM_HEIGHT lines
    int lines_printed = 0;
    long buffer_offset = b->view_offset;
    while (lines_printed < TERM_HEIGHT &&
	   buffer_offset-b->curr_page->offset < PAGE_SIZE_BYTES) {
	int buffer_index = buffer_offset-b->curr_page->offset;
	char c = b->curr_page->buffer[buffer_index];
	write(STDOUT_FILENO, &c, 1);
	if (c == '\n') lines_printed++;
	buffer_offset++;
    }
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void
buffer_close(Buffer *b)
{
    fclose(b->f);
    page_free(b->curr_page);
    free(b);
}

void
buffer_cursor_down(Buffer *b)
{
    // find the next newline, or TERM_WIDTH, whichever comes first
    int search_idx = b->cursor_text_index;
    while (b->curr_page[search_idx-b->curr_page->offset] != '\n' &&
	search_idx-b->curr_page->offset < TERM_WIDTH) {
	search_idx++;
    }

    // Find out where the next index will be
    write(STDOUT_FILENO, "\x1b[B", 3);
    b->cursor_y++;
}

void
buffer_cursor_up(Buffer *b)
{
    write(STDOUT_FILENO, "\x1b[A", 3);
    b->cursor_y++;
}

void
buffer_cursor_left(Buffer *b)
{
    b->cursor_x--;
    if (b->cursor_x < 0 && b->cursor_y == 0) {
	return;
    } else if (b->cursor_x < 0) {
	b->cursor_y--;
    }
    b->cursor_text_index--;

    write(STDOUT_FILENO, "\x1b[D", 3);
    b->cursor_x--;
}

void
buffer_cursor_right(Buffer *b)
{
    write(STDOUT_FILENO, "\x1b[C", 3);
    b->cursor_x++;
}


void
buffer_jump_to_middle(Buffer *b)
{
    fseek(b->f, 0L, SEEK_END);
    long file_size = ftell(b->f);
    long middle = file_size / 2L;
    b->view_offset = middle;

    page_free(b->curr_page);
    b->curr_page = page_read(b->f, middle);
    buffer_draw(b);
}

void
buffer_jump_to_end(Buffer *b)
{
    fseek(b->f, 0L, SEEK_END);
    long file_size = ftell(b->f);
    long page_offset = file_size - PAGE_SIZE_BYTES;
    if (page_offset < 0) page_offset = 0;
    b->view_offset = page_offset;

    page_free(b->curr_page);
    b->curr_page = page_read(b->f, page_offset);
    buffer_draw(b);
}

void
buffer_jump_to_start(Buffer *b)
{
    page_free(b->curr_page);
    b->view_offset = 0;
    b->curr_page = page_read(b->f, 0);
    buffer_draw(b);
}


/*
  When navigating the cursor:
  cursor down:
    find the next newline character
    advance current X past that newline character
    update cursor Y (+1)
    maintain cursor X
    
if the cursor moves off screen, advance the byte offset to just after
the first newline character found from the current offset.

In the future, decouple page loads from current buffer offset

*/

int
main()
{
    enable_raw();

    Buffer *b = buffer_load(FILENAME);
    buffer_draw(b);

    char c;
    while (1) {
	c = getchar();
	switch (c) {
	case EOF:
	    goto end;
	case 'q':
	    goto end;
	case 'j':
	    buffer_cursor_down(b);
	    break;
	case 'k':
	    buffer_cursor_up(b);
	    break;
	case 'h':
	    buffer_cursor_left(b);
	    break;
	case 'l':
	    buffer_cursor_right(b);
	    break;
	case 'L':
	    buffer_jump_to_middle(b);
	    break;
	case 'K':
	    buffer_jump_to_start(b);
	    break;
	case 'J':
	    buffer_jump_to_end(b);
	    break;
	default:
	    printf("%d", c);
	}
    }

end:

    buffer_close(b);

    return 0;
}
