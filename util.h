#pragma once

#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


struct global_config {
	int screenrows;
	int srceencols;
	struct termios glob_state;
};

int getWindowSize (int *rows, int *cols);// return 0 if success else return -1
void initEditor();
void die(char* msg);
