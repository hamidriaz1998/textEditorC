/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** data ***/
struct termios original_state;

/*** terminal ***/
void die(const char *s) {
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_state) == -1)
    die("tcsetattr failed");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &original_state) == -1)
    die("tcgetattr failed");
  atexit(disableRawMode);

  struct termios raw = original_state;
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

/*** init ***/
int main(int argc, char **argv) {
  enableRawMode();

  while (1) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
      die("read failed");
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == 'q') {
      break;
    }
  }
  return 0;
}
