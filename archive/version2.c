#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

#define CTRL_KEY(k) ((k) & 0x1f)

struct editorConfig {
	int screenrows;
	int screencols;
	struct termios orig_termios;
};

struct editorConfig E;

struct abuf {
	char* b;
	int len;
};

#define ABUF_INIT {NULL,0}

void abAppend(struct abuf *ab, const char* s, int len) {
	char *new = realloc(ab->b,ab->len + len);

	if (new == NULL) return ;

	memcpy(&new[ab->len],s,len);
	ab->b = new;
	ab->len += len;
}

void abFree(struct abuf *ab) {
	free(ab->b);
}

int getWindowSize(int *rows,int *cols) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws) == -1 || ws.ws_col == 0) {
		return -1;
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		
		return 0;
	}
}

void die(const char* s) {
	write(STDOUT_FILENO,"\x1b[2j",4);
	write(STDOUT_FILENO,"\x1b[H",3);
	perror(s);
	exit(1);
}

void initEditor() {
	if (getWindowSize(&E.screenrows,&E.screencols) == -1) die("getWindowSize");
}

void disableRawMode(struct termios term) {
	tcsetattr(STDIN_FILENO,TCSAFLUSH,&term);
}

void enableRawMode() {
	struct termios raw;

	tcgetattr(STDIN_FILENO,&raw);

	raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP| ICRNL | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw);

	tcgetattr(STDIN_FILENO,&E.orig_termios); // global state of terminal
}

char editorReadKey() {
	int nread;
	char c;
	while((nread = read(STDIN_FILENO,&c,1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}

	return c;
}

void editorProcessKeypress(struct termios term) {
	char c = editorReadKey();

	switch(c) {
		case CTRL_KEY('q'):
			write(STDOUT_FILENO,"\x1b[2j",4);
			write(STDOUT_FILENO,"\x1b[H",3);

			disableRawMode(term);

			exit(0);
			break;
	}
}

void editorDrawRows(struct abuf* ab) {
	int y;

	for (y = 0; y < E.screenrows; y++) {
//		write(STDOUT_FILENO,"~",1);
		abAppend(ab,"~",1);

		if (y < E.screenrows - 1 ) {
//			write(STDOUT_FILENO,"\r\n",2);
			abAppend(ab,"\r\n",2);
		}
	}
}


void editorRefreshScreen() {
	struct abuf ab = ABUF_INIT;

//	write(STDOUT_FILENO,"\x1b[2J",4);
//	write(STDOUT_FILENO,"\x1b[H",3);

	abAppend(&ab, "\x1b[?25l",6); // hiding the cursor before the refresh
	abAppend(&ab, "\x1b[2J",4);
	abAppend(&ab, "\x1b[h",3);

	editorDrawRows(&ab);
	
	abAppend(&ab, "\x1b[H",3);
	abAppend(&ab, "\x1b[?25h",6); // showing the cursor after the refresh

	write(STDOUT_FILENO,ab.b,ab.len);

	abFree(&ab);
}

int main() {
	struct termios term;
	tcgetattr(STDIN_FILENO,&term);

	enableRawMode();
	initEditor(); // geting the terminal width and height for render ~ in the editor
	char c;
	// while (read(STDIN_FILENO,&c,1) == 1 && c != 'q') {
	// 	if (iscntrl(c)) {
	// 		printf("%d\r\n",c);
	// 	} else {
	// 		printf("%d ('%c')\r\n",c,c);
	// 	}
	// }

	while (1) {
		// char c = '\0';
		// read(STDIN_FILENO,&c,1);

		// if (iscntrl(c)) {
		// 	printf("%d\r\n",c);
		// } else {
		// 	printf("%d ('%c')\r\n",c,c);
		// }
		// if (c == CTRL_KEY('q')) break;

		editorRefreshScreen();
		editorProcessKeypress(term);
	}
//	disableRawMode(term);	 // Right now I don't need this portion cause I am using editorProcessKeypress() function for that purpose
	return 0;
}
