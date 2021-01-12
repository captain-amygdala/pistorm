enum keypress_type {
  KEYPRESS_RELEASE,
  KEYPRESS_PRESS,
  KEYPRESS_REPEAT,
};

int get_mouse_status(char *x, char *y, char *b);
int get_key_char(char *c, char *code, char *event_type);
int queue_keypress(uint8_t keycode, uint8_t event_type, uint8_t platform);
int get_num_kb_queued();
void pop_queued_key(uint8_t *c, uint8_t *t);
