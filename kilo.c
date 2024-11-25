/*** includes ***/
#include <asm-generic/ioctls.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"

/*** data ***/
struct editorConfig {
  struct termios orig_term_state;
  int screenRows;
  int screenCols;
};

struct editorConfig E;

/*** terminal ***/
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_term_state) == -1)
    die("tcsetattr failed");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_term_state) == -1)
    die("tcgetattr failed");
  atexit(disableRawMode);

  struct termios raw = E.orig_term_state;
  // ICRNL flag is responsible for converting \r to \n
  // IXON flag is responsible for Ctrl-S and Ctrl-Q
  // BRKINT flag is responsible for sending SIGINT signal to the process
  // INPCK flag is responsible for enabling parity checking
  // ISTRIP flag is responsible for stripping 8th bit of each input byte
  // IXON flag is responsible for Ctrl-S and Ctrl-Q
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  // Disables OPOST flag which is responsible for output processing like \n to
  // \r\n
  raw.c_oflag &= ~(OPOST);
  // CS8 flag is responsible for setting character size to 8 bits per byte
  raw.c_cflag |= (CS8);
  // Disables Echo, Canonical Mode, Signals(Ctrl-C, Ctrl-Z) and Ctrl-V
  // IEXTEN flag is responsible for Ctrl-V
  // ISIG flag is responsible for Ctrl-C and Ctrl-Z
  // ICANON flag is responsible for Canonical Mode
  // ECHO flag is responsible for Echo
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  // VMIN flag is responsible for setting minimum number of bytes to read
  raw.c_cc[VMIN] = 0;
  // VTIME flag is responsible for setting maximum time to wait before reading.
  // In this case it is 1/10th of a second
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr failed");
}

char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }
  return c;
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\1xb[6n", 4) != 4) {
    return -1;
  }

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) {
      break;
    }
    if (buf[i] == 'R') {
      break;
    }
    i++;
  }
  buf[i] = '\0';
  printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** Append Buffer ***/
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL)
    return;

  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

/*** output ***/
void editorDrawRows(struct abuf *ab) {
  for (int i = 0; i < E.screenRows; i++) {
    if (i == E.screenRows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
                                "Kilo editor -- version %s", KILO_VERSION);
      if (welcomelen > E.screenCols)
        welcomelen = E.screenCols;
      int padding = (E.screenCols - welcomelen) / 2;
      if (padding) {
        abAppend(ab, "~", 1);
        padding--;
      }
      while (padding--)
        abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomelen);
    } else
      abAppend(ab, "~", 1);
    // Clear the current line to the right of the cursor
    abAppend(ab, "\x1b[K", 3);
    if (i < E.screenRows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  // Hide Cursor
  abAppend(&ab, "\x1b[?25l", 6);
  // Reposition Cursor to the top left
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  // Reposition Cursor to the top left
  abAppend(&ab, "\x1b[H", 3);
  // Show Cursor
  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** input ***/
void editorProcessInput() {
  char c = editorReadKey();

  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  }
}

/*** init ***/
void initEditor() {
  if (getWindowSize(&E.screenRows, &E.screenCols) == -1) {
    die("getWindowSize failed");
  }
}

int main(int argc, char **argv) {
  initEditor();
  enableRawMode();

  while (1) {
    editorRefreshScreen();
    editorProcessInput();
  }
  return 0;
}
