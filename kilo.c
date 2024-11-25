/*** includes ***/
#include <asm-generic/ioctls.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

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

/*** output ***/
void editorDrawRows() {
  for (int i = 0; i < E.screenRows; i++) {
    write(STDOUT_FILENO, "~", 1);
    if (i < E.screenRows - 1) {
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
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
