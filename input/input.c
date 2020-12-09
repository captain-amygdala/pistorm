#include <termios.h>
#include <unistd.h>
#include <linux/input.h>

static int lshift = 0, rshift = 0, lctrl = 0, rctrl = 0, lalt = 0, altgr = 0;
extern int mouse_fd;
extern int keyboard_fd;

enum keypress_type {
  KEYPRESS_RELEASE,
  KEYPRESS_PRESS,
  KEYPRESS_REPEAT,
};

int handle_modifier(struct input_event *ev) {
  int *target_modifier = NULL;
  if (ev->value != KEYPRESS_REPEAT && (ev->code == KEY_LEFTSHIFT || ev->code == KEY_RIGHTSHIFT || ev->code == KEY_LEFTALT || ev->code == KEY_RIGHTALT || ev->code == KEY_LEFTCTRL || ev->code == KEY_RIGHTCTRL)) {
    switch(ev->code) {
      case KEY_LEFTSHIFT:
        target_modifier = &lshift;
        break;
      case KEY_RIGHTSHIFT:
        target_modifier = &rshift;
        break;
      case KEY_LEFTALT:
        target_modifier = &lalt;
        break;
      case KEY_RIGHTALT:
        target_modifier = &altgr;
        break;
      case KEY_LEFTCTRL:
        target_modifier = &lshift;
        break;
      case KEY_RIGHTCTRL:
        target_modifier = &rshift;
        break;
    }
    *target_modifier = (ev->value == KEYPRESS_RELEASE) ? 0 : 1;
    return 1;
  }
  else {
    return 0;
  }
}

#define KEYCASE(a, b, c)case a: return (lshift || rshift) ? c : b;

char char_from_input_event(struct input_event *ev) {
  switch(ev->code) {
    KEYCASE(KEY_A, 'a', 'A');
    KEYCASE(KEY_B, 'b', 'B');
    KEYCASE(KEY_C, 'c', 'C');
    KEYCASE(KEY_D, 'd', 'D');
    KEYCASE(KEY_E, 'e', 'E');
    KEYCASE(KEY_F, 'f', 'F');
    KEYCASE(KEY_G, 'g', 'G');
    KEYCASE(KEY_H, 'h', 'H');
    KEYCASE(KEY_I, 'i', 'I');
    KEYCASE(KEY_J, 'j', 'J');
    KEYCASE(KEY_K, 'k', 'K');
    KEYCASE(KEY_L, 'l', 'L');
    KEYCASE(KEY_M, 'm', 'M');
    KEYCASE(KEY_N, 'n', 'N');
    KEYCASE(KEY_O, 'o', 'O');
    KEYCASE(KEY_P, 'p', 'P');
    KEYCASE(KEY_Q, 'q', 'Q');
    KEYCASE(KEY_R, 'r', 'R');
    KEYCASE(KEY_S, 's', 'S');
    KEYCASE(KEY_T, 't', 'T');
    KEYCASE(KEY_U, 'u', 'U');
    KEYCASE(KEY_V, 'c', 'V');
    KEYCASE(KEY_W, 'w', 'W');
    KEYCASE(KEY_X, 'x', 'X');
    KEYCASE(KEY_Y, 'y', 'Y');
    KEYCASE(KEY_Z, 'z', 'Z');
    KEYCASE(KEY_1, '1', '!');
    KEYCASE(KEY_2, '2', '@');
    KEYCASE(KEY_3, '3', '#');
    KEYCASE(KEY_4, '4', '$');
    KEYCASE(KEY_5, '5', '%');
    KEYCASE(KEY_6, '6', '^');
    KEYCASE(KEY_7, '7', '&');
    KEYCASE(KEY_8, '8', '*');
    KEYCASE(KEY_9, '9', '(');
    KEYCASE(KEY_0, '0', ')');
    KEYCASE(KEY_F12, 0x1B, 0x1B);
    default:
      return 0;
  }
}

int get_key_char(char *c)
{
  if (keyboard_fd == -1)
    return 0;

  struct input_event ie;
  while(read(keyboard_fd, &ie, sizeof(struct input_event)) != -1) {
    if (ie.type == EV_KEY) {
      if (handle_modifier(&ie))
        continue;
      char ret = char_from_input_event(&ie);
      if (ret != 0) {
        *c = ret;
        return 1;
      }
    }
  }

  return 0;
}

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
