#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "1.0.0.alpha"

struct editorConfig {
	int cx, cy;
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

enum editorKey {
  ARROW_LEFT = 'a',
  ARROW_RIGHT = 'd',
  ARROW_UP = 'w',
  ARROW_DOWN = 's'
};

void editorMoveCursor(char key) {
	switch(key) {
		case ARROW_LEFT:
			E.cx--;
			break;
		case ARROW_RIGHT:
			E.cx++;
			break;
		case ARROW_UP:
			E.cy--;
			break;
		case ARROW_DOWN:
			E.cy++;
			break;
	}
}

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
	E.cx = 0;
	E.cy = 0;
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

	if (c == '\x1b') {
		char seq[3];

		if (read(STDIN_FILENO,&seq[0],1) != 1) return '\x1b';
		if (read(STDIN_FILENO,&seq[1],1) != 1) return '\x1b';

		if (seq[0] == '[') {
			switch(seq[1]) {
				case 'A': return ARROW_UP;
				case 'B': return ARROW_DOWN;
				case 'C': return ARROW_RIGHT;
				case 'D': return ARROW_LEFT;
			}
		}

		return '\x1b';
	} else {
		return c;
	}
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
			
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
	}
}

void editorDrawRows(struct abuf* ab) {
	int y;

	for (y = 0; y < E.screenrows; y++) {
		if ( y == E.screenrows / 3) {
			char welcome[80];
			int welcomelen = snprintf(welcome,sizeof(welcome), "Kilo editor -- version %s",KILO_VERSION);
		
			if (welcomelen  > E.screencols) welcomelen = E.screencols;

			int padding = (E.screencols - welcomelen ) / 2;
			if (padding) {
				abAppend(ab,"~",1);
				padding--;
			}
			while (padding--) abAppend(ab," ",1);
			abAppend(ab,welcome,welcomelen);

		} else {
//		write(STDOUT_FILENO,"~",1);
			abAppend(ab,"~",1);
		}

		abAppend(ab,"\x1b[K",3);

		if (y < E.screenrows - 1 ) {
//			write(STDOUT_FILENO,"\r\n",2);
			abAppend(ab,"\r\n",2);
		}
	}
}


void editorRefreshScreen() {
	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l",6); // hiding the cursor before the refresh
	abAppend(&ab, "\x1b[h",3);

	editorDrawRows(&ab);

	char buf[32];
	snprintf(buf,sizeof(buf), "\x1b[%d;%dH",E.cy+1,E.cx + 1);
	abAppend(&ab,buf,strlen(buf));
	
//	abAppend(&ab, "\x1b[H",3);
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
