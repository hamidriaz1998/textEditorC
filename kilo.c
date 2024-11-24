#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios original_state;

void disableRawMode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_state); }

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &original_state);
  atexit(disableRawMode);

  struct termios raw = original_state;
  // Disables Ctrl-M(m is 13 alpha it should print 13 but was printing 10 bcz of
  // \r char converted to \n(10)) and (Ctrl-S and Ctrl-Q)
  raw.c_iflag &= ~(ICRNL | IXON);
  // Disables OPOST flag which is responsible for output processing like \n to
  // \r\n
  raw.c_oflag &= ~(OPOST);
  // Disables Echo, Canonical Mode, Signals(Ctrl-C, Ctrl-Z) and Ctrl-V
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(int argc, char **argv) {
  enableRawMode();

  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    if (iscntrl(c)) {
      printf("%d\n", c);
    } else {
      printf("%d ('%c')\n", c, c);
    }
  }
  return 0;
}
