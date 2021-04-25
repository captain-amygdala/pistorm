// SPDX-License-Identifier: MIT

unsigned int pi_find_pistorm();

unsigned short pi_get_hw_rev();
unsigned short pi_get_sw_rev();
unsigned short pi_get_net_status();
unsigned short pi_get_rtg_status();
unsigned short pi_get_piscsi_status();

void pi_enable_rtg(unsigned short val);
void pi_enable_net(unsigned short val);
void pi_enable_piscsi(unsigned short val);

void pi_reset_amiga(unsigned short reset_code);
unsigned short pi_handle_config(unsigned char cmd, char *str);

void pi_set_feature_status(unsigned short cmd, unsigned char value);

unsigned short pi_piscsi_map_drive(char *filename, unsigned char index);
unsigned short pi_piscsi_unmap_drive(unsigned char index);
unsigned short pi_piscsi_insert_media(char *filename, unsigned char index);
unsigned short pi_piscsi_eject_media(unsigned char index);

unsigned short pi_get_filesize(char *filename, unsigned int *file_size);
unsigned short pi_transfer_file(char *filename, unsigned char *dest_ptr);

unsigned short pi_load_config(char *filename);
void pi_reload_config();
void pi_load_default_config();

unsigned short pi_remap_kickrom(char *filename);
unsigned short pi_remap_extrom(char *filename);
