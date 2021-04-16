// SPDX-License-Identifier: MIT

#include <stdint.h>

enum keypress_type {
  KEYPRESS_RELEASE,
  KEYPRESS_PRESS,
  KEYPRESS_REPEAT,
};

int get_mouse_status(uint8_t *x, uint8_t *y, uint8_t *b, uint8_t *e);
int get_key_char(char *c, char *code, char *event_type);
int queue_keypress(uint8_t keycode, uint8_t event_type, uint8_t platform);
int get_num_kb_queued();
void pop_queued_key(uint8_t *c, uint8_t *t);
int grab_device(int fd);
int release_device(int fd);
