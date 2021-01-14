#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <linux/input.h>
#include "../platforms/platforms.h"
#include "input.h"

#define NONE 0x80

static int lshift = 0, rshift = 0, lctrl = 0, rctrl = 0, lalt = 0, altgr = 0;
extern int mouse_fd;
extern int keyboard_fd;

char keymap_amiga[256] = { \
/*      00    01    02    03    04    05    06    07    08    09    0A    0B    0C    0D    0E    0F */ \
/*00*/ 0x80, 0x45, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x41, 0x42, \
/*10*/ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x44, 0x63, 0x20, 0x21, \
/*20*/ 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x00, 0x60, 0x2B, 0x31, 0x32, 0x33, 0x34, \
/*30*/ 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x61, 0x5D, 0x64, 0x40, NONE, 0x50, 0x51, 0x52, 0x53, 0x54, \
/*40*/ 0x55, 0x56, 0x57, 0x58, 0x69, 0x5B, NONE, 0x3D, 0x3E, 0x3F, 0x4A, 0x2D, 0x2E, 0x2F, 0x4E, 0x1D, \
/*50*/ 0x1E, 0x1F, 0x0F, 0x3C, NONE, NONE, 0x30, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, \
/*60*/ 0x43, NONE, 0x5C, NONE, NONE, NONE, 0x5F, 0x4C, 0x5A, 0x4F, 0x4E, NONE, 0x4D, NONE, 0x0D, 0x46, \
/*70*/ NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, 0x66, 0x67, NONE, \
/*80*/ NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, \
/*90*/ NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, \
/*A0*/ NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, \
/*B0*/ NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, \
/*C0*/ NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, \
/*D0*/ NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, \
/*E0*/ NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, \
/*F0*/ NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE }; \

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

int get_key_char(char *c, char *code, char *event_type)
{
  if (keyboard_fd == -1)
    return 0;

  struct input_event ie;
  while(read(keyboard_fd, &ie, sizeof(struct input_event)) != -1) {
    if (ie.type == EV_KEY) {
      handle_modifier(&ie);
      char ret = char_from_input_event(&ie);
      *c = ret;
      *code = ie.code;
      *event_type = ie.value;
      return 1;
    }
  }

  return 0;
}

uint16_t mouse_x = 0, mouse_y = 0, mouse_b = 0;

int get_mouse_status(char *x, char *y, char *b) {
  struct input_event ie;
  if (read(mouse_fd, &ie, sizeof(struct input_event)) != -1) {
    *b = ((char *)&ie)[0];
    mouse_x += ((char *)&ie)[1];
    *x = mouse_x;
    mouse_y += (-((char *)&ie)[2]);
    *y = mouse_y; //-((char *)&ie)[2];
    return 1;
  }

  return 0;
}

static uint8_t queued_keypresses = 0, queue_output_pos = 0, queue_input_pos = 0;
static uint8_t queued_keys[256];
static uint8_t queued_events[256];

void clear_keypress_queue() {
  memset(queued_keys, 0x80, 256);
  memset(queued_events, 0x80, 256);
  queued_keypresses = 0;
  queue_output_pos = 0;
  queue_input_pos = 0;
}

int queue_keypress(uint8_t keycode, uint8_t event_type, uint8_t platform) {
  char *keymap = NULL;
  switch (platform) {
    case PLATFORM_AMIGA:
      if (event_type != KEYPRESS_REPEAT)
        keymap = keymap_amiga;
      break;
    default:
      break;
  }
  if (keymap != NULL) {
    if (keymap[keycode] != NONE) {
      if (queued_keypresses < 255) {
        //printf("Keypress queued, matched %.2X to host key code %.2X\n", keycode, keymap[keycode]);
        queued_keys[queue_output_pos] = keymap[keycode];
        queued_events[queue_output_pos] = event_type;
        queue_output_pos++;
        queued_keypresses++;
        return 1;
      }
    }
  }
  return 0;
}

int get_num_kb_queued() {
  return queued_keypresses;
}

void pop_queued_key(uint8_t *c, uint8_t *t) {
  if (queued_keypresses == 0) {
    *c = NONE;
    *t = NONE;
    return;
  }
  *c = queued_keys[queue_input_pos];
  *t = queued_events[queue_input_pos];
  queue_input_pos++;
  queued_keypresses--;
  return;
}
