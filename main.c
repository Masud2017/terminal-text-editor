#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <termios.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "1.0.0.alpha"

typedef struct erow {
	int size;
	char* chars;
}erow;

struct editorConfig {
	int cx, cy;
	int rowoff;
	int screenrows;
	int screencols;
	struct termios orig_termios;

	int numrows;
	erow *row; // making row as array of struct of erow
};

struct editorConfig E;

struct abuf {
	char* b;
	int len;
};

#define ABUF_INIT {NULL,0}

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

void die(const char* s) {
	write(STDOUT_FILENO,"\x1b[2j",4);
	write(STDOUT_FILENO,"\x1b[H",3);
	perror(s);
	exit(1);
}

/* row operations */
void editorAppendRow(char* s, size_t len) {
	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

	int at = E.numrows;
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars,s, len);
	E.row[at].chars[len] = '\0';
	E.numrows++;
}

/* File i/o */
void editorOpen(char* fileName) {
	FILE* fp = fopen(fileName,"r");

	if (!fp) die("fopen");

	char* line = NULL;
	size_t linecap = 0;
	ssize_t linelen;

	linelen = getline(&line, &linecap,fp);

	if (linelen != -1) {
		while ((linelen = getline(&line, &linecap, fp)) != -1) {
			while (linelen > 0 && (line[linelen - 1] == '\n' || 
					line[linelen - 1] == '\r'))
			linelen--;
			editorAppendRow(line, linelen);
		}
		// E.row->size = linelen;
		// E.row->chars = malloc(linelen + 1);
		// memcpy (E.row->chars, line , linelen);
		// E.row->chars[linelen] = '\0';
		// E.numrows = 1;
	}

	free(line);
	fclose(fp);
}

void editorMoveCursor(int key) {
	switch(key) {
		case ARROW_LEFT:
			if (E.cx != 0) {
				E.cx--;
			}
			break;
		case ARROW_RIGHT:
			if (E.cx != E.screencols - 1) {
				E.cx++;
			}
			break;
		case ARROW_UP:
			if (E.cy != 0) {
				E.cy--;
			}
			break;
		case ARROW_DOWN:
			if (E.cy != E.screenrows - 1) {
				E.cy++;
			}
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

void initEditor() {
	E.cx = 0;
	E.cy = 0;
	E.rowoff = 0;
	E.numrows = 0;
	E.row = NULL;

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

int editorReadKey() {
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
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(STDIN_FILENO,&seq[2],1) != 1) return '\x1b';
				if (seq[2] == '~') {
					switch(seq[1]) {
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			} else {
				switch(seq[1]) {
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
			} 
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}

		return '\x1b';
	} else {
		return c;
	}
}

void editorProcessKeypress(struct termios term) {
	int c = editorReadKey();

	switch(c) {
		case CTRL_KEY('q'):
			write(STDOUT_FILENO,"\x1b[2j",4);
			write(STDOUT_FILENO,"\x1b[H",3);

			disableRawMode(term);

			exit(0);
			break;
			
		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			E.cx = E.screencols - 1;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
				int times = E.screenrows;
				while (times--) {
					editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_UP);
				}
			}
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
		int filerow = y + E.rowoff;
		if (filerow >= E.numrows) {
			if (E.numrows == 0 && y == E.screenrows / 3) {
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
				abAppend(ab,"~",1);
			}

		} else {
			int len = E.row[filerow].size;
			if (len > E.screencols) len = E.screencols;
			abAppend(ab,E.row[filerow].chars,len);
		}

		abAppend(ab,"\x1b[K",3);

		if (y < E.screenrows - 1 ) {
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

int main(int argc, char** argv) {
	struct termios term;
	tcgetattr(STDIN_FILENO,&term);

	enableRawMode();
	initEditor(); // geting the terminal width and height for render ~ in the editor

	if (argc >= 2) {
		editorOpen(argv[1]);
	}

	char c;

	while (1) {

		editorRefreshScreen();
		editorProcessKeypress(term);
	}
	return 0;
}
