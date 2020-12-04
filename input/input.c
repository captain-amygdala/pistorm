#include <termios.h>
#include <unistd.h>
#include <linux/input.h>

int kbhit()
{
  return 0;
}

extern int mouse_fd;

int get_mouse_status(char *x, char *y, char *b) {
  struct input_event ie;
  if (read(mouse_fd, &ie, sizeof(struct input_event)) != -1) {
    *b = ((char *)&ie)[0];
    *x = ((char *)&ie)[1];
    *y = ((char *)&ie)[2];
    return 1;
  }

  return 0;
}
