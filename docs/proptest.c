#include <termios.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

int main(argc, argv)
int argc;
char *argv[];
{
  struct termios save_termios;
  struct termios buf;
  Display *dpy;
  Window window;
  unsigned long window_atom;
  char c;
  if (argc < 2) {
    printf("Usage: %s windowid\n", argv[0]);
    exit(-1);
  }
  window = (Window)strtol(argv[1], NULL, 0);
  dpy = XOpenDisplay(NULL);
  if (dpy == NULL)
    exit(-1);
  window_atom = XInternAtom(dpy, "XANIM_PROPERTY", 0);

  if (tcgetattr(0, &save_termios) < 0)
    exit(-1);
  buf = save_termios;
  buf.c_lflag &= ~(ECHO | ICANON);
  buf.c_cc[VMIN] = 1;
  buf.c_cc[VTIME] = 0;
  if (tcsetattr(0, TCSAFLUSH, &buf) < 0)
    exit(-1);
  do {
    c = getchar();
    printf("sending char %c\n", c);
    XChangeProperty(dpy, window, window_atom, XA_STRING, 8,
		    PropModeReplace, (unsigned char *)&c, 1);
    XFlush(dpy);
  } while (c != 'q');
  XCloseDisplay(dpy);
  if (tcsetattr(0, TCSAFLUSH, &save_termios) < 0)
    exit(-1);
}
