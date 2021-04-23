// SPDX-License-Identifier: MIT

unsigned int pi_find_pistorm();

unsigned short pi_get_hw_rev();
unsigned short pi_get_sw_rev();
unsigned short pi_get_net_status();
unsigned short pi_get_rtg_status();

void pi_reset_amiga(unsigned short reset_code);
void pi_handle_config(unsigned char cmd, char *str);
