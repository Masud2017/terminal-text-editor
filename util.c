#include "util.h"

struct global_config E;

int getWindowSize(int *rows, int *cols) {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ,&ws) == -1 || ws.ws_col == 0) return -1
	else {
		*rows = ws.ws_row;
		*cols = ws.ws_col;

		return 0;
	}
}

void initEditor() {
	if (getWindowSize(&E.rows,&E.cols) == -1) perror("getWindow(): ");
}

void die(char* msg) {
	write(STDOUT_FILENO, "\x1b[j",4);
	write(STDOUT_FILENO, "\x1b[H",3);
	perror(msg);
	exit(1);
}
