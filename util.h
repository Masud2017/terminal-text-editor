#pragma once

#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <termios.h>

#define CRL_ALT(x) ((x) & 0x1f)

struct global_config {
	int screenrows;
	int srceencols;
	struct termios glob_state;
};

int getWindowSize (int *rows, int *cols);// return 0 if success else return -1
void initEditor();
void die(char* msg);
